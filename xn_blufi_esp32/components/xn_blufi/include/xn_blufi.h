/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: BluFi蓝牙配网组件 - 头文件
 * 
 * 功能说明：
 * 1. 通过蓝牙接收WiFi配置信息
 * 2. 管理WiFi连接、断开、扫描
 * 3. 保存和删除WiFi配置到NVS
 * 4. 面向对象设计，易于使用
 */

#ifndef XN_BLUFI_H
#define XN_BLUFI_H

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 引入WiFi管理器和存储层的类型定义
#include "xn_wifi_manager.h"
#include "xn_wifi_storage.h"

/* BluFi配网组件类 */
typedef struct xn_blufi_s xn_blufi_t;

/**
 * @brief 创建BluFi配网组件实例
 * @param device_name 蓝牙设备名称，将显示在小程序中
 * @return 组件实例指针，失败返回NULL
 */
xn_blufi_t* xn_blufi_create(const char *device_name);

/**
 * @brief 销毁BluFi配网组件实例
 * @param blufi 组件实例指针
 */
void xn_blufi_destroy(xn_blufi_t *blufi);

/**
 * @brief 初始化BluFi配网组件
 * @param blufi 组件实例指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_init(xn_blufi_t *blufi);

/**
 * @brief 反初始化BluFi配网组件
 * @param blufi 组件实例指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_deinit(xn_blufi_t *blufi);

/**
 * @brief 连接到指定WiFi
 * @param blufi 组件实例指针
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_wifi_connect(xn_blufi_t *blufi, const char *ssid, const char *password);

/**
 * @brief 断开当前WiFi连接
 * @param blufi 组件实例指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_wifi_disconnect(xn_blufi_t *blufi);

/**
 * @brief 保存WiFi配置到NVS
 * @param blufi 组件实例指针
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_wifi_save(xn_blufi_t *blufi, const char *ssid, const char *password);

/**
 * @brief 从NVS删除WiFi配置
 * @param blufi 组件实例指针
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_wifi_delete(xn_blufi_t *blufi);

/**
 * @brief 从NVS加载WiFi配置
 * @param blufi 组件实例指针
 * @param config 输出参数，保存加载的配置
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_wifi_load(xn_blufi_t *blufi, xn_wifi_config_t *config);

/**
 * @brief 扫描周围WiFi
 * @param blufi 组件实例指针
 * @param callback 扫描完成回调函数
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_blufi_wifi_scan(xn_blufi_t *blufi, xn_wifi_scan_done_cb_t callback);

/**
 * @brief 获取当前WiFi连接状态
 * @param blufi 组件实例指针
 * @return WiFi连接状态
 */
xn_wifi_status_t xn_blufi_wifi_get_status(xn_blufi_t *blufi);

/**
 * @brief 注册WiFi状态变化回调
 * @param blufi 组件实例指针
 * @param callback 状态变化回调函数
 */
void xn_blufi_wifi_register_status_cb(xn_blufi_t *blufi, xn_wifi_status_cb_t callback);

/**
 * @brief 获取蓝牙连接状态
 * @param blufi 组件实例指针
 * @return true表示已连接，false表示未连接
 */
bool xn_blufi_is_ble_connected(xn_blufi_t *blufi);

#ifdef __cplusplus
}
#endif

#endif // XN_BLUFI_H
