/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: BluFi GATT服务器实现 - NimBLE版本
 */

#include "xn_blufi_internal.h"
#include "esp_log.h"
#include "esp_blufi.h"

static const char *TAG = "XN_BLUFI_GATT"; // 日志标签

/* 初始化GATT服务器 */
int xn_blufi_gatt_svr_init(void)
{
    // BluFi协议栈会自动初始化GATT服务器
    // 这里只需要初始化BluFi的GATT服务
    int ret = esp_blufi_gatt_svr_init();
    if (ret != 0) {
        ESP_LOGE(TAG, "初始化BluFi GATT服务失败: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "GATT服务器初始化成功");
    return 0;
}

/* 反初始化GATT服务器 */
void xn_blufi_gatt_svr_deinit(void)
{
    // BluFi协议栈会自动清理GATT服务
    esp_blufi_gatt_svr_deinit();
    ESP_LOGI(TAG, "GATT服务器已反初始化");
}

/* GATT服务器注册回调 */
void xn_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    // BluFi协议栈会处理GATT注册
    esp_blufi_gatt_svr_register_cb(ctxt, arg);
}
