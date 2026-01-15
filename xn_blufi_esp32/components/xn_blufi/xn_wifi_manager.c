/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-15
 * @Description: WiFi管理层 - 实现文件
 */

#include "xn_wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "XN_WIFI_MANAGER";

#define WIFI_CONNECTED_BIT BIT0
#define MAX_RETRY_COUNT 5

/* WiFi管理器实例结构体 */
struct xn_wifi_manager_s {
    EventGroupHandle_t event_group;         // WiFi事件组
    xn_wifi_status_t status;                // WiFi连接状态
    xn_wifi_scan_done_cb_t scan_callback;   // 扫描完成回调
    xn_wifi_status_cb_t status_callback;    // 状态变化回调
    wifi_config_t wifi_config;              // WiFi配置
    uint8_t retry_count;                    // 重连计数
    bool is_connecting;                     // 是否正在连接
    esp_netif_t *netif;                     // 网络接口
};

/* 更新WiFi状态并触发回调 */
static void update_status(xn_wifi_manager_t *manager, xn_wifi_status_t new_status)
{
    if (manager->status != new_status) {
        manager->status = new_status;
        ESP_LOGI(TAG, "WiFi状态变化: %d", new_status);
        if (manager->status_callback) {
            manager->status_callback(new_status);
        }
    }
}

/* WiFi事件处理函数 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    xn_wifi_manager_t *manager = (xn_wifi_manager_t *)arg;
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi已启动");
                break;
                
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t*)event_data;
                ESP_LOGI(TAG, "已连接到WiFi: %s", event->ssid);
                update_status(manager, XN_WIFI_CONNECTED);
                manager->is_connecting = false;
                manager->retry_count = 0;
                break;
            }
            
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t*)event_data;
                ESP_LOGW(TAG, "WiFi断开，原因: %d", event->reason);
                
                // 如果正在连接且未超过重试次数，则重连
                if (manager->is_connecting && manager->retry_count < MAX_RETRY_COUNT) {
                    esp_wifi_connect();
                    manager->retry_count++;
                    ESP_LOGI(TAG, "重连WiFi，第%d次", manager->retry_count);
                    update_status(manager, XN_WIFI_CONNECTING);
                } else {
                    manager->is_connecting = false;
                    update_status(manager, XN_WIFI_DISCONNECTED);
                }
                xEventGroupClearBits(manager->event_group, WIFI_CONNECTED_BIT);
                break;
            }
            
            case WIFI_EVENT_SCAN_DONE: {
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count);
                
                if (ap_count == 0) {
                    ESP_LOGW(TAG, "未扫描到WiFi");
                    if (manager->scan_callback) {
                        manager->scan_callback(0, NULL);
                    }
                    break;
                }
                
                wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
                if (ap_list == NULL) {
                    ESP_LOGE(TAG, "分配内存失败");
                    break;
                }
                
                esp_wifi_scan_get_ap_records(&ap_count, ap_list);
                ESP_LOGI(TAG, "扫描到%d个WiFi", ap_count);
                
                if (manager->scan_callback) {
                    manager->scan_callback(ap_count, ap_list);
                }
                
                free(ap_list);
                break;
            }
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "获取到IP: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(manager->event_group, WIFI_CONNECTED_BIT);
            update_status(manager, XN_WIFI_GOT_IP);
        }
    }
}

/* 创建WiFi管理器实例 */
xn_wifi_manager_t* xn_wifi_manager_create(void)
{
    xn_wifi_manager_t *manager = malloc(sizeof(xn_wifi_manager_t));
    if (manager == NULL) {
        ESP_LOGE(TAG, "分配内存失败");
        return NULL;
    }
    
    memset(manager, 0, sizeof(xn_wifi_manager_t));
    manager->status = XN_WIFI_DISCONNECTED;
    
    ESP_LOGI(TAG, "WiFi管理器创建成功");
    return manager;
}

/* 销毁WiFi管理器实例 */
void xn_wifi_manager_destroy(xn_wifi_manager_t *manager)
{
    if (manager) {
        if (manager->event_group) {
            vEventGroupDelete(manager->event_group);
        }
        free(manager);
        ESP_LOGI(TAG, "WiFi管理器已销毁");
    }
}

/* 初始化WiFi管理器 */
esp_err_t xn_wifi_manager_init(xn_wifi_manager_t *manager)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 创建事件组
    manager->event_group = xEventGroupCreate();
    if (manager->event_group == NULL) {
        ESP_LOGE(TAG, "创建事件组失败");
        return ESP_FAIL;
    }
    
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    manager->netif = esp_netif_create_default_wifi_sta();
    
    // 注册WiFi和IP事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, manager));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, manager));
    
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi管理器初始化成功");
    return ESP_OK;
}

/* 反初始化WiFi管理器 */
esp_err_t xn_wifi_manager_deinit(xn_wifi_manager_t *manager)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 停止WiFi
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // 注销事件处理函数
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    
    ESP_LOGI(TAG, "WiFi管理器已反初始化");
    return ESP_OK;
}

/* 连接WiFi */
esp_err_t xn_wifi_manager_connect(xn_wifi_manager_t *manager, 
                                   const char *ssid, 
                                   const char *password)
{
    if (manager == NULL || ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 设置WiFi配置
    memset(&manager->wifi_config, 0, sizeof(wifi_config_t));
    strncpy((char*)manager->wifi_config.sta.ssid, ssid, 
           sizeof(manager->wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)manager->wifi_config.sta.password, password, 
               sizeof(manager->wifi_config.sta.password) - 1);
    }
    
    // 断开当前连接
    esp_wifi_disconnect();
    
    // 设置新配置并连接
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &manager->wifi_config));
    manager->is_connecting = true;
    manager->retry_count = 0;
    update_status(manager, XN_WIFI_CONNECTING);
    
    ESP_LOGI(TAG, "开始连接WiFi: %s", ssid);
    return esp_wifi_connect();
}

/* 断开WiFi */
esp_err_t xn_wifi_manager_disconnect(xn_wifi_manager_t *manager)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    manager->is_connecting = false;
    ESP_LOGI(TAG, "断开WiFi连接");
    return esp_wifi_disconnect();
}

/* 扫描WiFi */
esp_err_t xn_wifi_manager_scan(xn_wifi_manager_t *manager, 
                                xn_wifi_scan_done_cb_t callback)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    manager->scan_callback = callback;
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };
    
    ESP_LOGI(TAG, "开始扫描WiFi");
    return esp_wifi_scan_start(&scan_config, false);
}

/* 获取WiFi状态 */
xn_wifi_status_t xn_wifi_manager_get_status(xn_wifi_manager_t *manager)
{
    if (manager == NULL) {
        return XN_WIFI_DISCONNECTED;
    }
    return manager->status;
}

/* 注册状态回调 */
void xn_wifi_manager_register_status_cb(xn_wifi_manager_t *manager, 
                                         xn_wifi_status_cb_t callback)
{
    if (manager) {
        manager->status_callback = callback;
    }
}
