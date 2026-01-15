/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-15
 * @Description: WiFi配置存储层 - 实现文件
 */

#include "xn_wifi_storage.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "XN_WIFI_STORAGE";
#define NVS_NAMESPACE "wifi_cfg"

/* 初始化WiFi存储层 */
esp_err_t xn_wifi_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS需要擦除，正在擦除...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi存储层初始化成功");
    } else {
        ESP_LOGE(TAG, "WiFi存储层初始化失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/* 保存WiFi配置到NVS */
esp_err_t xn_wifi_storage_save(const char *ssid, const char *password)
{
    if (ssid == NULL) {
        ESP_LOGE(TAG, "SSID不能为空");
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
    } else {
        // 如果密码为空，删除密码字段
        nvs_erase_key(nvs_handle, "password");
    }
    
    // 提交更改
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi配置已保存: %s", ssid);
    } else {
        ESP_LOGE(TAG, "提交NVS失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/* 从NVS加载WiFi配置 */
esp_err_t xn_wifi_storage_load(xn_wifi_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "配置指针不能为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    size_t len;
    
    // 清空配置
    memset(config, 0, sizeof(xn_wifi_config_t));
    
    // 打开NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "打开NVS失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 读取SSID
    len = sizeof(config->ssid);
    ret = nvs_get_str(nvs_handle, "ssid", config->ssid, &len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "读取SSID失败: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // 读取密码（可选）
    len = sizeof(config->password);
    ret = nvs_get_str(nvs_handle, "password", config->password, &len);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "读取密码失败: %s", esp_err_to_name(ret));
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "WiFi配置已加载: %s", config->ssid);
    return ESP_OK;
}

/* 从NVS删除WiFi配置 */
esp_err_t xn_wifi_storage_delete(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // 打开NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "打开NVS失败: %s", esp_err_to_name(ret));
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
    } else {
        ESP_LOGE(TAG, "删除WiFi配置失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/* 检查是否存在WiFi配置 */
bool xn_wifi_storage_exists(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    size_t len;
    
    // 打开NVS
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    // 检查SSID是否存在
    ret = nvs_get_str(nvs_handle, "ssid", NULL, &len);
    nvs_close(nvs_handle);
    
    return (ret == ESP_OK);
}
