/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: BluFi安全层实现 - DH密钥交换、AES加密解密、CRC校验
 */

#include "xn_blufi_internal.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_crc.h"
#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "XN_BLUFI_SEC"; // 日志标签

/* 安全层数据类型定义 */
#define SEC_TYPE_DH_PARAM_LEN   0x00  // DH参数长度
#define SEC_TYPE_DH_PARAM_DATA  0x01  // DH参数数据
#define SEC_TYPE_DH_P           0x02  // DH参数P
#define SEC_TYPE_DH_G           0x03  // DH参数G
#define SEC_TYPE_DH_PUBLIC      0x04  // DH公钥

#define DH_SELF_PUB_KEY_LEN     128   // 自身公钥长度
#define SHARE_KEY_LEN           128   // 共享密钥长度
#define PSK_LEN                 16    // 预共享密钥长度

/* 安全层上下文结构体 */
typedef struct {
    uint8_t self_public_key[DH_SELF_PUB_KEY_LEN]; // 自身公钥
    uint8_t share_key[SHARE_KEY_LEN];             // 共享密钥
    size_t share_len;                             // 共享密钥长度
    uint8_t psk[PSK_LEN];                         // 预共享密钥(PSK)
    uint8_t *dh_param;                            // DH参数缓冲区
    int dh_param_len;                             // DH参数长度
    uint8_t iv[16];                               // AES初始化向量
    mbedtls_dhm_context dhm;                      // DH上下文
    mbedtls_aes_context aes;                      // AES上下文
} xn_blufi_security_t;

static xn_blufi_security_t *g_security = NULL; // 全局安全上下文

/* 随机数生成函数 - mbedtls回调 */
static int myrand(void *rng_state, unsigned char *output, size_t len)
{
    esp_fill_random(output, len); // 使用ESP32硬件随机数生成器
    return 0;
}

/* 初始化安全层 */
int xn_blufi_security_init(void)
{
    // 分配安全上下文内存
    g_security = (xn_blufi_security_t *)malloc(sizeof(xn_blufi_security_t));
    if (g_security == NULL) {
        ESP_LOGE(TAG, "分配安全上下文内存失败");
        return -1;
    }
    
    memset(g_security, 0, sizeof(xn_blufi_security_t)); // 清零
    
    mbedtls_dhm_init(&g_security->dhm); // 初始化DH上下文
    mbedtls_aes_init(&g_security->aes); // 初始化AES上下文
    
    memset(g_security->iv, 0, sizeof(g_security->iv)); // 清零IV
    
    ESP_LOGI(TAG, "安全层初始化成功");
    return 0;
}

/* 反初始化安全层 */
void xn_blufi_security_deinit(void)
{
    if (g_security == NULL) {
        return;
    }
    
    // 释放DH参数缓冲区
    if (g_security->dh_param) {
        free(g_security->dh_param);
        g_security->dh_param = NULL;
    }
    
    mbedtls_dhm_free(&g_security->dhm); // 释放DH上下文
    mbedtls_aes_free(&g_security->aes); // 释放AES上下文
    
    memset(g_security, 0, sizeof(xn_blufi_security_t)); // 清零
    
    free(g_security); // 释放内存
    g_security = NULL;
    
    ESP_LOGI(TAG, "安全层已反初始化");
}

/* DH密钥协商数据处理函数 */
void xn_blufi_dh_negotiate_data_handler(uint8_t *data, int len,
                                        uint8_t **output_data, int *output_len,
                                        bool *need_free)
{
    if (data == NULL || len < 3) { // 数据格式检查
        ESP_LOGE(TAG, "无效的数据格式");
        return;
    }
    
    if (g_security == NULL) { // 安全层未初始化
        ESP_LOGE(TAG, "安全层未初始化");
        return;
    }
    
    uint8_t type = data[0]; // 获取数据类型
    int ret;
    
    switch (type) {
        case SEC_TYPE_DH_PARAM_LEN: { // 接收DH参数长度
            g_security->dh_param_len = (data[1] << 8) | data[2]; // 解析长度
            ESP_LOGI(TAG, "DH参数长度: %d", g_security->dh_param_len);
            
            // 释放旧的DH参数缓冲区
            if (g_security->dh_param) {
                free(g_security->dh_param);
            }
            
            // 分配新的DH参数缓冲区
            g_security->dh_param = (uint8_t *)malloc(g_security->dh_param_len);
            if (g_security->dh_param == NULL) {
                ESP_LOGE(TAG, "分配DH参数缓冲区失败");
                g_security->dh_param_len = 0;
                return;
            }
            break;
        }
        
        case SEC_TYPE_DH_PARAM_DATA: { // 接收DH参数数据
            if (g_security->dh_param == NULL) {
                ESP_LOGE(TAG, "DH参数缓冲区为空");
                return;
            }
            
            if (len < (g_security->dh_param_len + 1)) {
                ESP_LOGE(TAG, "DH参数长度不匹配");
                return;
            }
            
            // 复制DH参数
            memcpy(g_security->dh_param, &data[1], g_security->dh_param_len);
            
            // 读取DH参数(P和G)
            uint8_t *param = g_security->dh_param;
            ret = mbedtls_dhm_read_params(&g_security->dhm, &param, 
                                         &param[g_security->dh_param_len]);
            if (ret) {
                ESP_LOGE(TAG, "读取DH参数失败: %d", ret);
                return;
            }
            
            // 释放DH参数缓冲区
            free(g_security->dh_param);
            g_security->dh_param = NULL;
            
            // 获取DH长度
            const int dhm_len = mbedtls_dhm_get_len(&g_security->dhm);
            if (dhm_len > DH_SELF_PUB_KEY_LEN) {
                ESP_LOGE(TAG, "DH长度不支持: %d", dhm_len);
                return;
            }
            
            // 生成自身公钥
            ret = mbedtls_dhm_make_public(&g_security->dhm, dhm_len,
                                         g_security->self_public_key,
                                         DH_SELF_PUB_KEY_LEN, myrand, NULL);
            if (ret) {
                ESP_LOGE(TAG, "生成公钥失败: %d", ret);
                return;
            }
            
            // 计算共享密钥
            ret = mbedtls_dhm_calc_secret(&g_security->dhm,
                                         g_security->share_key,
                                         SHARE_KEY_LEN,
                                         &g_security->share_len,
                                         myrand, NULL);
            if (ret) {
                ESP_LOGE(TAG, "计算共享密钥失败: %d", ret);
                return;
            }
            
            // 使用MD5生成PSK
            ret = mbedtls_md5(g_security->share_key, g_security->share_len, 
                             g_security->psk);
            if (ret) {
                ESP_LOGE(TAG, "生成PSK失败: %d", ret);
                return;
            }
            
            // 设置AES加密密钥
            mbedtls_aes_setkey_enc(&g_security->aes, g_security->psk, PSK_LEN * 8);
            
            // 返回自身公钥
            *output_data = g_security->self_public_key;
            *output_len = dhm_len;
            *need_free = false;
            
            ESP_LOGI(TAG, "DH密钥协商完成");
            break;
        }
        
        default:
            ESP_LOGW(TAG, "未知的数据类型: %d", type);
            break;
    }
}

/* AES加密函数 */
int xn_blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    if (g_security == NULL) { // 安全层未初始化
        return -1;
    }
    
    size_t iv_offset = 0;
    uint8_t iv0[16];
    
    // 准备IV
    memcpy(iv0, g_security->iv, sizeof(g_security->iv));
    iv0[0] = iv8; // 设置IV的第一个字节
    
    // AES-CFB128加密
    int ret = mbedtls_aes_crypt_cfb128(&g_security->aes, MBEDTLS_AES_ENCRYPT,
                                      crypt_len, &iv_offset, iv0,
                                      crypt_data, crypt_data);
    if (ret) {
        ESP_LOGE(TAG, "AES加密失败: %d", ret);
        return -1;
    }
    
    return crypt_len;
}

/* AES解密函数 */
int xn_blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    if (g_security == NULL) { // 安全层未初始化
        return -1;
    }
    
    size_t iv_offset = 0;
    uint8_t iv0[16];
    
    // 准备IV
    memcpy(iv0, g_security->iv, sizeof(g_security->iv));
    iv0[0] = iv8; // 设置IV的第一个字节
    
    // AES-CFB128解密
    int ret = mbedtls_aes_crypt_cfb128(&g_security->aes, MBEDTLS_AES_DECRYPT,
                                      crypt_len, &iv_offset, iv0,
                                      crypt_data, crypt_data);
    if (ret) {
        ESP_LOGE(TAG, "AES解密失败: %d", ret);
        return -1;
    }
    
    return crypt_len;
}

/* CRC校验函数 */
uint16_t xn_blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len)
{
    // 使用ESP32的CRC16-BE计算校验和
    return esp_crc16_be(0, data, len);
}
