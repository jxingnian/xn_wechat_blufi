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
 * @brief 从NVS加载WiFi配置（加载第一个配置，兼容旧接口）
 * @param config 输出参数，保存加载的配置
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_storage_load(xn_wifi_config_t *config);

/**
 * @brief 从NVS加载所有WiFi配置
 * @param configs 输出参数，保存加载的配置数组
 * @param count 输出参数，实际加载的配置数量
 * @param max_count 最大加载数量
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_storage_load_all(xn_wifi_config_t *configs, uint8_t *count, uint8_t max_count);

/**
 * @brief 删除指定索引的WiFi配置
 * @param index 配置索引（从0开始）
 * @return ESP_OK成功，其他值失败
 */
esp_err_t xn_wifi_storage_delete_by_index(uint8_t index);

/**
 * @brief 从NVS删除所有WiFi配置
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
