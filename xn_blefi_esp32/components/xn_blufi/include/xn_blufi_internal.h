/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: BluFi内部头文件 - 安全层相关函数声明
 */

#ifndef XN_BLUFI_INTERNAL_H
#define XN_BLUFI_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DH密钥协商数据处理函数 */
void xn_blufi_dh_negotiate_data_handler(uint8_t *data, int len, 
                                        uint8_t **output_data, int *output_len, 
                                        bool *need_free);

/* AES加密函数 */
int xn_blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

/* AES解密函数 */
int xn_blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);

/* CRC校验函数 */
uint16_t xn_blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

/* 初始化安全层 */
int xn_blufi_security_init(void);

/* 反初始化安全层 */
void xn_blufi_security_deinit(void);

/* NimBLE重置回调 */
void xn_blufi_on_reset(int reason);

/* NimBLE同步回调 */
void xn_blufi_on_sync(void);

/* GATT服务器注册回调 */
void xn_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

/* 初始化GATT服务器 */
int xn_blufi_gatt_svr_init(void);

/* 反初始化GATT服务器 */
void xn_blufi_gatt_svr_deinit(void);

/* NimBLE主机任务 */
void xn_blufi_host_task(void *param);

#ifdef __cplusplus
}
#endif

#endif // XN_BLUFI_INTERNAL_H
