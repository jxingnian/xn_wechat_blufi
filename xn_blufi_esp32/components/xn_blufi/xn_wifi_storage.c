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
#define MAX_WIFI_CONFIGS 10  // 最多存储10个WiFi配置

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

/* 保存WiFi配置到NVS（添加到列表） */
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
    
    // 读取当前配置数量
    uint8_t count = 0;
    nvs_get_u8(nvs_handle, "count", &count);
    
    // 检查是否已存在相同SSID
    int existing_index = -1;
    for (int i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "ssid_%d", i);
        
        size_t len = 64;
        char stored_ssid[64];
        if (nvs_get_str(nvs_handle, key, stored_ssid, &len) == ESP_OK) {
            if (strcmp(stored_ssid, ssid) == 0) {
                existing_index = i;
                break;
            }
        }
    }
    
    // 如果已存在，更新密码；否则添加新配置
    int index = (existing_index >= 0) ? existing_index : count;
    
    if (existing_index < 0 && count >= MAX_WIFI_CONFIGS) {
        ESP_LOGW(TAG, "WiFi配置已满，删除最旧的配置");
        // 删除第一个配置，所有配置前移
        for (int i = 0; i < count - 1; i++) {
            char old_ssid_key[16], old_pwd_key[16];
            char new_ssid_key[16], new_pwd_key[16];
            
            snprintf(old_ssid_key, sizeof(old_ssid_key), "ssid_%d", i + 1);
            snprintf(old_pwd_key, sizeof(old_pwd_key), "pwd_%d", i + 1);
            snprintf(new_ssid_key, sizeof(new_ssid_key), "ssid_%d", i);
            snprintf(new_pwd_key, sizeof(new_pwd_key), "pwd_%d", i);
            
            size_t len = 64;
            char temp[64];
            
            if (nvs_get_str(nvs_handle, old_ssid_key, temp, &len) == ESP_OK) {
                nvs_set_str(nvs_handle, new_ssid_key, temp);
            }
            
            len = 64;
            if (nvs_get_str(nvs_handle, old_pwd_key, temp, &len) == ESP_OK) {
                nvs_set_str(nvs_handle, new_pwd_key, temp);
            }
        }
        index = count - 1;
    } else if (existing_index < 0) {
        count++;
    }
    
    // 保存SSID和密码
    char ssid_key[16], pwd_key[16];
    snprintf(ssid_key, sizeof(ssid_key), "ssid_%d", index);
    snprintf(pwd_key, sizeof(pwd_key), "pwd_%d", index);
    
    ret = nvs_set_str(nvs_handle, ssid_key, ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存SSID失败: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    if (password) {
        ret = nvs_set_str(nvs_handle, pwd_key, password);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "保存密码失败: %s", esp_err_to_name(ret));
            nvs_close(nvs_handle);
            return ret;
        }
    } else {
        nvs_erase_key(nvs_handle, pwd_key);
    }
    
    // 保存配置数量
    nvs_set_u8(nvs_handle, "count", count);
    
    // 提交更改
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi配置已保存 [%d/%d]: %s", index + 1, count, ssid);
    } else {
        ESP_LOGE(TAG, "提交NVS失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/* 从NVS加载第一个WiFi配置（兼容旧接口） */
esp_err_t xn_wifi_storage_load(xn_wifi_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "配置指针不能为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    memset(config, 0, sizeof(xn_wifi_config_t));
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 读取第一个配置
    size_t len = sizeof(config->ssid);
    ret = nvs_get_str(nvs_handle, "ssid_0", config->ssid, &len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    len = sizeof(config->password);
    nvs_get_str(nvs_handle, "pwd_0", config->password, &len);
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "WiFi配置已加载: %s", config->ssid);
    return ESP_OK;
}

/* 加载所有WiFi配置 */
esp_err_t xn_wifi_storage_load_all(xn_wifi_config_t *configs, uint8_t *count, uint8_t max_count)
{
    if (configs == NULL || count == NULL) {
        ESP_LOGE(TAG, "参数不能为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    *count = 0;
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 读取配置数量
    uint8_t stored_count = 0;
    nvs_get_u8(nvs_handle, "count", &stored_count);
    
    // 读取所有配置
    for (int i = 0; i < stored_count && i < max_count; i++) {
        char ssid_key[16], pwd_key[16];
        snprintf(ssid_key, sizeof(ssid_key), "ssid_%d", i);
        snprintf(pwd_key, sizeof(pwd_key), "pwd_%d", i);
        
        size_t len = sizeof(configs[i].ssid);
        if (nvs_get_str(nvs_handle, ssid_key, configs[i].ssid, &len) == ESP_OK) {
            len = sizeof(configs[i].password);
            nvs_get_str(nvs_handle, pwd_key, configs[i].password, &len);
            (*count)++;
        }
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "已加载%d个WiFi配置", *count);
    return ESP_OK;
}

/* 删除指定索引的WiFi配置 */
esp_err_t xn_wifi_storage_delete_by_index(uint8_t index)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "打开NVS失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 读取配置数量
    uint8_t count = 0;
    nvs_get_u8(nvs_handle, "count", &count);
    
    if (index >= count) {
        ESP_LOGW(TAG, "索引超出范围: %d >= %d", index, count);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 删除指定配置，后面的配置前移
    for (int i = index; i < count - 1; i++) {
        char old_ssid_key[16], old_pwd_key[16];
        char new_ssid_key[16], new_pwd_key[16];
        
        snprintf(old_ssid_key, sizeof(old_ssid_key), "ssid_%d", i + 1);
        snprintf(old_pwd_key, sizeof(old_pwd_key), "pwd_%d", i + 1);
        snprintf(new_ssid_key, sizeof(new_ssid_key), "ssid_%d", i);
        snprintf(new_pwd_key, sizeof(new_pwd_key), "pwd_%d", i);
        
        size_t len = 64;
        char temp[64];
        
        if (nvs_get_str(nvs_handle, old_ssid_key, temp, &len) == ESP_OK) {
            nvs_set_str(nvs_handle, new_ssid_key, temp);
        }
        
        len = 64;
        if (nvs_get_str(nvs_handle, old_pwd_key, temp, &len) == ESP_OK) {
            nvs_set_str(nvs_handle, new_pwd_key, temp);
        }
    }
    
    // 删除最后一个配置的键
    char last_ssid_key[16], last_pwd_key[16];
    snprintf(last_ssid_key, sizeof(last_ssid_key), "ssid_%d", count - 1);
    snprintf(last_pwd_key, sizeof(last_pwd_key), "pwd_%d", count - 1);
    nvs_erase_key(nvs_handle, last_ssid_key);
    nvs_erase_key(nvs_handle, last_pwd_key);
    
    // 更新数量
    count--;
    nvs_set_u8(nvs_handle, "count", count);
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi配置已删除，索引: %d", index);
    } else {
        ESP_LOGE(TAG, "删除WiFi配置失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/* 从NVS删除所有WiFi配置 */
esp_err_t xn_wifi_storage_delete(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "打开NVS失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 读取配置数量
    uint8_t count = 0;
    nvs_get_u8(nvs_handle, "count", &count);
    
    // 删除所有配置
    for (int i = 0; i < count; i++) {
        char ssid_key[16], pwd_key[16];
        snprintf(ssid_key, sizeof(ssid_key), "ssid_%d", i);
        snprintf(pwd_key, sizeof(pwd_key), "pwd_%d", i);
        nvs_erase_key(nvs_handle, ssid_key);
        nvs_erase_key(nvs_handle, pwd_key);
    }
    
    nvs_erase_key(nvs_handle, "count");
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "所有WiFi配置已删除");
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
    
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    uint8_t count = 0;
    ret = nvs_get_u8(nvs_handle, "count", &count);
    nvs_close(nvs_handle);
    
    return (ret == ESP_OK && count > 0);
}
