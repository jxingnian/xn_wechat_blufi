/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-15
 * @Description: WiFi管理层 - 头文件
 * 
 * 功能说明：
 * 1. 负责WiFi连接、断开、扫描
 * 2. 管理WiFi状态和重连逻辑
 * 3. 处理WiFi事件并通知上层
 */

#ifndef XN_WIFI_MANAGER_H
#define XN_WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WiFi连接状态 */
typedef enum {
    XN_WIFI_DISCONNECTED = 0,   // 未连接
    XN_WIFI_CONNECTING,         // 连接中
    XN_WIFI_CONNECTED,          // 已连接但未获取IP
    XN_WIFI_GOT_IP              // 已连接并获取IP
} xn_wifi_status_t;

/* WiFi扫描结果回调函数类型 */
typedef void (*xn_wifi_scan_done_cb_t)(uint16_t ap_count, wifi_ap_record_t *ap_list);

/* WiFi状态变化回调函数类型 */
typedef void (*xn_wifi_status_cb_t)(xn_wifi_status_t status);

/* WiFi管理器实例 */
typedef struct xn_wifi_manager_s xn_wifi_manager_t;

/**
 * @brief 创建WiFi管理器实例
 * @return 管理器实例指针，失败返回NULL
 */
xn_wifi_manager_t* xn_wifi_manager_create(void);

/**
 * @brief 销毁WiFi管理器实例
 * @param manager 管理器实例指针
 */
void xn_wifi_manager_destroy(xn_wifi_manager_t *manager);

/**
 * @brief 初始化WiFi管理器
 * @param manager 管理器实例指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_manager_init(xn_wifi_manager_t *manager);

/**
 * @brief 反初始化WiFi管理器
 * @param manager 管理器实例指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_manager_deinit(xn_wifi_manager_t *manager);

/**
 * @brief 连接到指定WiFi
 * @param manager 管理器实例指针
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_manager_connect(xn_wifi_manager_t *manager, 
                                   const char *ssid, 
                                   const char *password);

/**
 * @brief 断开当前WiFi连接
 * @param manager 管理器实例指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_manager_disconnect(xn_wifi_manager_t *manager);

/**
 * @brief 扫描周围WiFi
 * @param manager 管理器实例指针
 * @param callback 扫描完成回调函数
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_manager_scan(xn_wifi_manager_t *manager, 
                                xn_wifi_scan_done_cb_t callback);

/**
 * @brief 获取当前WiFi连接状态
 * @param manager 管理器实例指针
 * @return WiFi连接状态
 */
xn_wifi_status_t xn_wifi_manager_get_status(xn_wifi_manager_t *manager);

/**
 * @brief 注册WiFi状态变化回调
 * @param manager 管理器实例指针
 * @param callback 状态变化回调函数
 */
void xn_wifi_manager_register_status_cb(xn_wifi_manager_t *manager, 
                                         xn_wifi_status_cb_t callback);

#ifdef __cplusplus
}
#endif

#endif // XN_WIFI_MANAGER_H
