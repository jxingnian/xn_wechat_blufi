/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: BluFi GATT服务器实现 - NimBLE版本
 */

#include "xn_blufi_internal.h"
#include "esp_log.h"
#include "esp_blufi.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "XN_BLUFI_GATT"; // 日志标签

/* BluFi服务UUID */
#define BLUFI_SERVICE_UUID          0xFFFF
#define BLUFI_CHAR_P2E_UUID         0xFF01  // 手机到ESP32
#define BLUFI_CHAR_E2P_UUID         0xFF02  // ESP32到手机

/* GATT服务器回调函数 */
static int xn_blufi_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);

/* GATT服务定义 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLUFI_SERVICE_UUID), // BluFi服务UUID
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(BLUFI_CHAR_P2E_UUID), // 写特征值
                .access_cb = xn_blufi_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLUFI_CHAR_E2P_UUID), // 通知特征值
                .access_cb = xn_blufi_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                0, // 结束标记
            }
        },
    },
    {
        0, // 结束标记
    },
};

/* GATT特征值访问回调 */
static int xn_blufi_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    
    // 判断是哪个特征值
    if (ble_uuid_cmp(uuid, BLE_UUID16_DECLARE(BLUFI_CHAR_P2E_UUID)) == 0) {
        // 手机写入数据
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            ESP_LOGI(TAG, "收到数据，长度: %d", OS_MBUF_PKTLEN(ctxt->om));
            // 将数据传递给BluFi协议栈处理
            esp_blufi_recv_data(ctxt->om->om_data, OS_MBUF_PKTLEN(ctxt->om));
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, BLE_UUID16_DECLARE(BLUFI_CHAR_E2P_UUID)) == 0) {
        // ESP32发送数据到手机（通知）
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            // 读操作（通常不需要处理）
            return 0;
        }
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/* 初始化GATT服务器 */
int xn_blufi_gatt_svr_init(void)
{
    int ret;
    
    // 重置GATT服务器
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    // 注册GATT服务
    ret = ble_gatts_count_cfg(gatt_svr_svcs);
    if (ret != 0) {
        ESP_LOGE(TAG, "GATT服务计数失败: %d", ret);
        return ret;
    }
    
    ret = ble_gatts_add_svcs(gatt_svr_svcs);
    if (ret != 0) {
        ESP_LOGE(TAG, "添加GATT服务失败: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "GATT服务器初始化成功");
    return 0;
}

/* 反初始化GATT服务器 */
void xn_blufi_gatt_svr_deinit(void)
{
    // NimBLE会自动清理GATT服务
    ESP_LOGI(TAG, "GATT服务器已反初始化");
}
