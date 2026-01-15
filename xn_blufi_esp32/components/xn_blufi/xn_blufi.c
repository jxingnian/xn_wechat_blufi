/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-15
 * @Description: BluFi蓝牙配网组件 - 实现文件（重构版）
 */

#include "xn_blufi.h"
#include "xn_blufi_internal.h"
#include "xn_wifi_manager.h"
#include "xn_wifi_storage.h"
#include "esp_log.h"
#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_store.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h>

static const char *TAG = "XN_BLUFI";

/* BluFi组件实例结构体 */
struct xn_blufi_s {
    char device_name[32];                   // 蓝牙设备名称
    xn_wifi_manager_t *wifi_manager;        // WiFi管理器
    bool ble_connected;                     // 蓝牙是否已连接
    char pending_ssid[32];                  // 待连接的SSID
    char pending_password[64];              // 待连接的密码
};

static xn_blufi_t *g_blufi_instance = NULL;

/* NimBLE重置回调 */
void xn_blufi_on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE重置，原因: %d", reason);
}

/* NimBLE同步回调 */
void xn_blufi_on_sync(void)
{
    ESP_LOGI(TAG, "NimBLE同步完成");
    esp_blufi_profile_init();
}

/* NimBLE主机任务 */
void xn_blufi_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE主机任务启动");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* BluFi事件回调函数 */
static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    xn_blufi_t *blufi = g_blufi_instance;
    if (blufi == NULL) return;
    
    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(TAG, "BluFi初始化完成");
            esp_blufi_adv_start();
            break;
            
        case ESP_BLUFI_EVENT_DEINIT_FINISH:
            ESP_LOGI(TAG, "BluFi反初始化完成");
            break;
            
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            ESP_LOGI(TAG, "蓝牙已连接");
            blufi->ble_connected = true;
            esp_blufi_adv_stop();
            xn_blufi_security_init();
            break;
            
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            ESP_LOGI(TAG, "蓝牙断开连接");
            blufi->ble_connected = false;
            xn_blufi_security_deinit();
            esp_blufi_adv_start();
            break;
            
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            strncpy(blufi->pending_ssid, (char*)param->sta_ssid.ssid, 
                   param->sta_ssid.ssid_len);
            blufi->pending_ssid[param->sta_ssid.ssid_len] = '\0';
            ESP_LOGI(TAG, "接收到SSID: %s", blufi->pending_ssid);
            break;
            
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            strncpy(blufi->pending_password, (char*)param->sta_passwd.passwd,
                   param->sta_passwd.passwd_len);
            blufi->pending_password[param->sta_passwd.passwd_len] = '\0';
            ESP_LOGI(TAG, "接收到密码");
            break;
            
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
            ESP_LOGI(TAG, "请求连接WiFi");
            xn_wifi_manager_connect(blufi->wifi_manager, 
                                   blufi->pending_ssid, 
                                   blufi->pending_password);
            break;
            
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(TAG, "请求断开WiFi");
            xn_wifi_manager_disconnect(blufi->wifi_manager);
            break;
            
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            
            xn_wifi_status_t status = xn_wifi_manager_get_status(blufi->wifi_manager);
            if (status == XN_WIFI_GOT_IP) {
                esp_blufi_extra_info_t info = {0};
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
            } else if (status == XN_WIFI_CONNECTING) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, 0, NULL);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
            }
            break;
        }
        
        case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
            ESP_LOGI(TAG, "请求扫描WiFi");
            wifi_scan_config_t scan_config = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false
            };
            esp_wifi_scan_start(&scan_config, false);
            break;
        }
        
        default:
            break;
    }
}

/* BluFi回调函数结构体 */
static esp_blufi_callbacks_t blufi_callbacks = {
    .event_cb = blufi_event_callback,
    .negotiate_data_handler = xn_blufi_dh_negotiate_data_handler,
    .encrypt_func = xn_blufi_aes_encrypt,
    .decrypt_func = xn_blufi_aes_decrypt,
    .checksum_func = xn_blufi_crc_checksum,
};

/* 创建BluFi实例 */
xn_blufi_t* xn_blufi_create(const char *device_name)
{
    xn_blufi_t *blufi = malloc(sizeof(xn_blufi_t));
    if (blufi == NULL) {
        ESP_LOGE(TAG, "分配内存失败");
        return NULL;
    }
    
    memset(blufi, 0, sizeof(xn_blufi_t));
    strncpy(blufi->device_name, device_name, sizeof(blufi->device_name) - 1);
    
    // 创建WiFi管理器
    blufi->wifi_manager = xn_wifi_manager_create();
    if (blufi->wifi_manager == NULL) {
        ESP_LOGE(TAG, "创建WiFi管理器失败");
        free(blufi);
        return NULL;
    }
    
    ESP_LOGI(TAG, "BluFi实例创建成功");
    return blufi;
}

/* 销毁BluFi实例 */
void xn_blufi_destroy(xn_blufi_t *blufi)
{
    if (blufi) {
        if (blufi->wifi_manager) {
            xn_wifi_manager_destroy(blufi->wifi_manager);
        }
        free(blufi);
        ESP_LOGI(TAG, "BluFi实例已销毁");
    }
}

/* 初始化BluFi */
esp_err_t xn_blufi_init(xn_blufi_t *blufi)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_blufi_instance = blufi;
    esp_err_t ret;
    
    // 初始化存储层
    ret = xn_wifi_storage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化存储层失败");
        return ret;
    }
    
    // 初始化WiFi管理器
    ret = xn_wifi_manager_init(blufi->wifi_manager);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化WiFi管理器失败");
        return ret;
    }
    
    // 初始化NimBLE协议栈
    ret = esp_nimble_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化NimBLE失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置NimBLE主机
    ble_hs_cfg.reset_cb = xn_blufi_on_reset;
    ble_hs_cfg.sync_cb = xn_blufi_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    
    // 初始化GATT服务器
    ret = esp_blufi_gatt_svr_init();
    if (ret != 0) {
        ESP_LOGE(TAG, "初始化GATT服务器失败");
        return ESP_FAIL;
    }
    
    // 设置设备名称
    ret = ble_svc_gap_device_name_set(blufi->device_name);
    if (ret != 0) {
        ESP_LOGE(TAG, "设置设备名称失败");
        return ESP_FAIL;
    }
    
    // 初始化BluFi BTC层
    esp_blufi_btc_init();
    
    // 注册BluFi回调
    ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(TAG, "注册BluFi回调失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 启动NimBLE主机任务
    ret = esp_nimble_enable(xn_blufi_host_task);
    if (ret) {
        ESP_LOGE(TAG, "启动NimBLE失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BluFi初始化成功");
    return ESP_OK;
}

/* 反初始化BluFi */
esp_err_t xn_blufi_deinit(xn_blufi_t *blufi)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 反初始化GATT服务器
    esp_blufi_gatt_svr_deinit();
    
    // 停止NimBLE
    esp_err_t ret = nimble_port_stop();
    if (ret == ESP_OK) {
        nimble_port_deinit();
        ret = esp_nimble_deinit();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "反初始化NimBLE失败");
        }
    }
    
    // 反初始化BluFi profile
    esp_blufi_profile_deinit();
    
    // 反初始化BluFi BTC层
    esp_blufi_btc_deinit();
    
    // 反初始化WiFi管理器
    xn_wifi_manager_deinit(blufi->wifi_manager);
    
    g_blufi_instance = NULL;
    
    ESP_LOGI(TAG, "BluFi反初始化完成");
    return ESP_OK;
}

/* 连接WiFi - 委托给WiFi管理器 */
esp_err_t xn_blufi_wifi_connect(xn_blufi_t *blufi, const char *ssid, const char *password)
{
    if (blufi == NULL || blufi->wifi_manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xn_wifi_manager_connect(blufi->wifi_manager, ssid, password);
}

/* 断开WiFi - 委托给WiFi管理器 */
esp_err_t xn_blufi_wifi_disconnect(xn_blufi_t *blufi)
{
    if (blufi == NULL || blufi->wifi_manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xn_wifi_manager_disconnect(blufi->wifi_manager);
}

/* 保存WiFi配置 - 委托给存储层 */
esp_err_t xn_blufi_wifi_save(xn_blufi_t *blufi, const char *ssid, const char *password)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xn_wifi_storage_save(ssid, password);
}

/* 删除WiFi配置 - 委托给存储层 */
esp_err_t xn_blufi_wifi_delete(xn_blufi_t *blufi)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xn_wifi_storage_delete();
}

/* 加载WiFi配置 - 委托给存储层 */
esp_err_t xn_blufi_wifi_load(xn_blufi_t *blufi, xn_wifi_config_t *config)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xn_wifi_storage_load(config);
}

/* 扫描WiFi - 委托给WiFi管理器 */
esp_err_t xn_blufi_wifi_scan(xn_blufi_t *blufi, xn_wifi_scan_done_cb_t callback)
{
    if (blufi == NULL || blufi->wifi_manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xn_wifi_manager_scan(blufi->wifi_manager, callback);
}

/* 获取WiFi状态 - 委托给WiFi管理器 */
xn_wifi_status_t xn_blufi_wifi_get_status(xn_blufi_t *blufi)
{
    if (blufi == NULL || blufi->wifi_manager == NULL) {
        return XN_WIFI_DISCONNECTED;
    }
    return xn_wifi_manager_get_status(blufi->wifi_manager);
}

/* 注册状态回调 - 委托给WiFi管理器 */
void xn_blufi_wifi_register_status_cb(xn_blufi_t *blufi, xn_wifi_status_cb_t callback)
{
    if (blufi && blufi->wifi_manager) {
        xn_wifi_manager_register_status_cb(blufi->wifi_manager, callback);
    }
}
