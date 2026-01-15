/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-15
 * @Description: WiFi配置存储层 - 头文件
 * 
 * 功能说明：
 * 1. 负责WiFi配置的持久化存储
 * 2. 使用NVS存储SSID和密码
 * 3. 提供保存、加载、删除接口
 */

#ifndef XN_WIFI_STORAGE_H
#define XN_WIFI_STORAGE_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WiFi配置信息结构体 */
typedef struct {
    char ssid[32];          // WiFi名称
    char password[64];      // WiFi密码
} xn_wifi_config_t;

/**
 * @brief 初始化WiFi存储层
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_storage_init(void);

/**
 * @brief 保存WiFi配置到NVS
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_storage_save(const char *ssid, const char *password);

/**
 * @brief 从NVS加载WiFi配置
 * @param config 输出参数，保存加载的配置
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_storage_load(xn_wifi_config_t *config);

/**
 * @brief 从NVS删除WiFi配置
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_storage_delete(void);

/**
 * @brief 检查是否存在WiFi配置
 * @return true存在，false不存在
 */
bool xn_wifi_storage_exists(void);

#ifdef __cplusplus
}
#endif

#endif // XN_WIFI_STORAGE_H
