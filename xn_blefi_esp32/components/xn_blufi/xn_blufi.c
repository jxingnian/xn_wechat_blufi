/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: BluFi蓝牙配网组件 - 实现文件
 */

#include "xn_blufi.h"
#include "xn_blufi_internal.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
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
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "XN_BLUFI"; // 日志标签

#define WIFI_CONNECTED_BIT BIT0  // WiFi已连接事件位
#define NVS_NAMESPACE "wifi_cfg" // NVS命名空间
#define MAX_RETRY_COUNT 5        // 最大重连次数

/* BluFi组件实例结构体 */
struct xn_blufi_s {
    char device_name[32];                   // 蓝牙设备名称
    EventGroupHandle_t wifi_event_group;    // WiFi事件组
    xn_wifi_status_t wifi_status;           // WiFi连接状态
    xn_wifi_scan_done_cb_t scan_callback;   // 扫描完成回调
    xn_wifi_status_cb_t status_callback;    // 状态变化回调
    wifi_config_t wifi_config;              // WiFi配置
    uint8_t retry_count;                    // 重连计数
    bool is_connecting;                     // 是否正在连接
    bool ble_connected;                     // 蓝牙是否已连接
};

static xn_blufi_t *g_blufi_instance = NULL; // 全局实例指针

/* NimBLE重置回调 */
void xn_blufi_on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE重置，原因: %d", reason);
}

/* NimBLE同步回调 */
void xn_blufi_on_sync(void)
{
    ESP_LOGI(TAG, "NimBLE同步完成");
    // 初始化BluFi profile
    esp_blufi_profile_init();
}

/* NimBLE主机任务 */
void xn_blufi_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE主机任务启动");
    // 运行NimBLE主机，此函数会阻塞直到nimble_port_stop()被调用
    nimble_port_run();
    // 清理资源
    nimble_port_freertos_deinit();
}

/* 更新WiFi状态并触发回调 */
static void update_wifi_status(xn_blufi_t *blufi, xn_wifi_status_t new_status)
{
    if (blufi->wifi_status != new_status) { // 状态发生变化
        blufi->wifi_status = new_status;
        ESP_LOGI(TAG, "WiFi状态变化: %d", new_status);
        if (blufi->status_callback) { // 如果注册了回调函数
            blufi->status_callback(new_status);
        }
    }
}

/* WiFi事件处理函数 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    xn_blufi_t *blufi = (xn_blufi_t *)arg;
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START: // WiFi启动
                ESP_LOGI(TAG, "WiFi已启动");
                break;
                
            case WIFI_EVENT_STA_CONNECTED: { // WiFi已连接
                wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t*)event_data;
                ESP_LOGI(TAG, "已连接到WiFi: %s", event->ssid);
                update_wifi_status(blufi, XN_WIFI_CONNECTED);
                blufi->is_connecting = false;
                blufi->retry_count = 0;
                break;
            }
            
            case WIFI_EVENT_STA_DISCONNECTED: { // WiFi断开连接
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t*)event_data;
                ESP_LOGW(TAG, "WiFi断开，原因: %d", event->reason);
                
                // 如果正在连接且未超过重试次数，则重连
                if (blufi->is_connecting && blufi->retry_count < MAX_RETRY_COUNT) {
                    esp_wifi_connect();
                    blufi->retry_count++;
                    ESP_LOGI(TAG, "重连WiFi，第%d次", blufi->retry_count);
                    update_wifi_status(blufi, XN_WIFI_CONNECTING);
                } else {
                    blufi->is_connecting = false;
                    update_wifi_status(blufi, XN_WIFI_DISCONNECTED);
                }
                xEventGroupClearBits(blufi->wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            }
            
            case WIFI_EVENT_SCAN_DONE: { // WiFi扫描完成
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count); // 获取扫描到的AP数量
                
                if (ap_count == 0) {
                    ESP_LOGW(TAG, "未扫描到WiFi");
                    if (blufi->scan_callback) {
                        blufi->scan_callback(0, NULL);
                    }
                    break;
                }
                
                // 分配内存存储扫描结果
                wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
                if (ap_list == NULL) {
                    ESP_LOGE(TAG, "分配内存失败");
                    break;
                }
                
                // 获取扫描结果
                esp_wifi_scan_get_ap_records(&ap_count, ap_list);
                ESP_LOGI(TAG, "扫描到%d个WiFi", ap_count);
                
                // 调用回调函数
                if (blufi->scan_callback) {
                    blufi->scan_callback(ap_count, ap_list);
                }
                
                free(ap_list); // 释放内存
                break;
            }
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) { // 获取到IP地址
            ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "获取到IP: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(blufi->wifi_event_group, WIFI_CONNECTED_BIT);
            update_wifi_status(blufi, XN_WIFI_GOT_IP);
        }
    }
}

/* BluFi事件回调函数 */
static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    xn_blufi_t *blufi = g_blufi_instance;
    if (blufi == NULL) return;
    
    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH: // BluFi初始化完成
            ESP_LOGI(TAG, "BluFi初始化完成");
            esp_blufi_adv_start(); // 开始广播
            break;
            
        case ESP_BLUFI_EVENT_DEINIT_FINISH: // BluFi反初始化完成
            ESP_LOGI(TAG, "BluFi反初始化完成");
            break;
            
        case ESP_BLUFI_EVENT_BLE_CONNECT: // 蓝牙已连接
            ESP_LOGI(TAG, "蓝牙已连接");
            blufi->ble_connected = true;
            esp_blufi_adv_stop(); // 停止广播
            xn_blufi_security_init(); // 初始化安全层
            break;
            
        case ESP_BLUFI_EVENT_BLE_DISCONNECT: // 蓝牙断开连接
            ESP_LOGI(TAG, "蓝牙断开连接");
            blufi->ble_connected = false;
            xn_blufi_security_deinit(); // 反初始化安全层
            esp_blufi_adv_start(); // 重新开始广播
            break;
            
        case ESP_BLUFI_EVENT_RECV_STA_SSID: // 接收到WiFi SSID
            strncpy((char*)blufi->wifi_config.sta.ssid, 
                   (char*)param->sta_ssid.ssid, 
                   param->sta_ssid.ssid_len);
            blufi->wifi_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            ESP_LOGI(TAG, "接收到SSID: %s", blufi->wifi_config.sta.ssid);
            break;
            
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD: // 接收到WiFi密码
            strncpy((char*)blufi->wifi_config.sta.password,
                   (char*)param->sta_passwd.passwd,
                   param->sta_passwd.passwd_len);
            blufi->wifi_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            ESP_LOGI(TAG, "接收到密码");
            break;
            
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP: // 请求连接WiFi
            ESP_LOGI(TAG, "请求连接WiFi");
            esp_wifi_disconnect(); // 先断开当前连接
            esp_wifi_set_config(WIFI_IF_STA, &blufi->wifi_config); // 设置新配置
            blufi->is_connecting = true;
            blufi->retry_count = 0;
            esp_wifi_connect(); // 开始连接
            update_wifi_status(blufi, XN_WIFI_CONNECTING);
            break;
            
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP: // 请求断开WiFi
            ESP_LOGI(TAG, "请求断开WiFi");
            esp_wifi_disconnect();
            break;
            
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS: { // 获取WiFi状态
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            
            // 根据当前状态发送报告
            if (blufi->wifi_status == XN_WIFI_GOT_IP) {
                esp_blufi_extra_info_t info = {0};
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
            } else if (blufi->wifi_status == XN_WIFI_CONNECTING) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, 0, NULL);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
            }
            break;
        }
        
        case ESP_BLUFI_EVENT_GET_WIFI_LIST: { // 请求扫描WiFi
            ESP_LOGI(TAG, "请求扫描WiFi");
            wifi_scan_config_t scan_config = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false
            };
            esp_wifi_scan_start(&scan_config, false); // 异步扫描
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
    xn_blufi_t *blufi = malloc(sizeof(xn_blufi_t)); // 分配内存
    if (blufi == NULL) {
        ESP_LOGE(TAG, "分配内存失败");
        return NULL;
    }
    
    memset(blufi, 0, sizeof(xn_blufi_t)); // 清零
    strncpy(blufi->device_name, device_name, sizeof(blufi->device_name) - 1); // 设置设备名
    blufi->wifi_status = XN_WIFI_DISCONNECTED; // 初始状态为未连接
    
    return blufi;
}

/* 销毁BluFi实例 */
void xn_blufi_destroy(xn_blufi_t *blufi)
{
    if (blufi) {
        if (blufi->wifi_event_group) {
            vEventGroupDelete(blufi->wifi_event_group); // 删除事件组
        }
        free(blufi); // 释放内存
    }
}

/* 初始化BluFi */
esp_err_t xn_blufi_init(xn_blufi_t *blufi)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_blufi_instance = blufi; // 保存全局实例
    esp_err_t ret;
    
    // 初始化NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 创建WiFi事件组
    blufi->wifi_event_group = xEventGroupCreate();
    if (blufi->wifi_event_group == NULL) {
        ESP_LOGE(TAG, "创建事件组失败");
        return ESP_FAIL;
    }
    
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta(); // 创建STA网络接口
    
    // 注册WiFi和IP事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, blufi));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, blufi));
    
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 设置为STA模式
    ESP_ERROR_CHECK(esp_wifi_start()); // 启动WiFi
    
    // 初始化NimBLE协议栈
    ret = esp_nimble_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化NimBLE失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置NimBLE主机
    ble_hs_cfg.reset_cb = xn_blufi_on_reset;
    ble_hs_cfg.sync_cb = xn_blufi_on_sync;
    ble_hs_cfg.gatts_register_cb = xn_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    
    // 初始化GATT服务器
    ret = xn_blufi_gatt_svr_init();
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
    xn_blufi_gatt_svr_deinit();
    
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
    
    // 停止WiFi
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // 注销事件处理函数
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    
    g_blufi_instance = NULL;
    
    ESP_LOGI(TAG, "BluFi反初始化完成");
    return ESP_OK;
}

/* 连接WiFi */
esp_err_t xn_blufi_wifi_connect(xn_blufi_t *blufi, const char *ssid, const char *password)
{
    if (blufi == NULL || ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 设置WiFi配置
    memset(&blufi->wifi_config, 0, sizeof(wifi_config_t));
    strncpy((char*)blufi->wifi_config.sta.ssid, ssid, sizeof(blufi->wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)blufi->wifi_config.sta.password, password, 
               sizeof(blufi->wifi_config.sta.password) - 1);
    }
    
    // 断开当前连接
    esp_wifi_disconnect();
    
    // 设置新配置并连接
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &blufi->wifi_config));
    blufi->is_connecting = true;
    blufi->retry_count = 0;
    update_wifi_status(blufi, XN_WIFI_CONNECTING);
    
    return esp_wifi_connect();
}

/* 断开WiFi */
esp_err_t xn_blufi_wifi_disconnect(xn_blufi_t *blufi)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    blufi->is_connecting = false;
    return esp_wifi_disconnect();
}

/* 保存WiFi配置到NVS */
esp_err_t xn_blufi_wifi_save(xn_blufi_t *blufi, const char *ssid, const char *password)
{
    if (blufi == NULL || ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // 打开NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 保存SSID
    ret = nvs_set_str(nvs_handle, "ssid", ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存SSID失败: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // 保存密码
    if (password) {
        ret = nvs_set_str(nvs_handle, "password", password);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "保存密码失败: %s", esp_err_to_name(ret));
            nvs_close(nvs_handle);
            return ret;
        }
    }
    
    // 提交更改
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi配置已保存");
    }
    
    return ret;
}

/* 从NVS删除WiFi配置 */
esp_err_t xn_blufi_wifi_delete(xn_blufi_t *blufi)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // 打开NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 删除SSID和密码
    nvs_erase_key(nvs_handle, "ssid");
    nvs_erase_key(nvs_handle, "password");
    
    // 提交更改
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi配置已删除");
    }
    
    return ret;
}

/* 从NVS加载WiFi配置 */
esp_err_t xn_blufi_wifi_load(xn_blufi_t *blufi, xn_wifi_config_t *config)
{
    if (blufi == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    size_t len;
    
    // 打开NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 读取SSID
    len = sizeof(config->ssid);
    ret = nvs_get_str(nvs_handle, "ssid", config->ssid, &len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    // 读取密码
    len = sizeof(config->password);
    ret = nvs_get_str(nvs_handle, "password", config->password, &len);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi配置已加载: %s", config->ssid);
    }
    
    return ret;
}

/* 扫描WiFi */
esp_err_t xn_blufi_wifi_scan(xn_blufi_t *blufi, xn_wifi_scan_done_cb_t callback)
{
    if (blufi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    blufi->scan_callback = callback; // 保存回调函数
    
    // 配置扫描参数
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };
    
    return esp_wifi_scan_start(&scan_config, false); // 异步扫描
}

/* 获取WiFi状态 */
xn_wifi_status_t xn_blufi_wifi_get_status(xn_blufi_t *blufi)
{
    if (blufi == NULL) {
        return XN_WIFI_DISCONNECTED;
    }
    return blufi->wifi_status;
}

/* 注册状态回调 */
void xn_blufi_wifi_register_status_cb(xn_blufi_t *blufi, xn_wifi_status_cb_t callback)
{
    if (blufi) {
        blufi->status_callback = callback;
    }
}
