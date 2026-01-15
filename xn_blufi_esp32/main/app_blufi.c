/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: 蓝牙配网应用层 - 实现文件
 */

#include "app_blufi.h"
#include "xn_blufi.h"
#include "esp_log.h"
#include "esp_blufi_api.h"
#include "esp_wifi.h"

static const char *TAG = "APP_BLUFI"; // 日志标签
static xn_blufi_t *g_blufi = NULL;    // BluFi实例

/* WiFi状态变化回调函数 */
static void wifi_status_callback(xn_wifi_status_t status)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    
    // 检查蓝牙是否已连接
    bool ble_connected = xn_blufi_is_ble_connected(g_blufi);
    
    switch(status) {
        case XN_WIFI_DISCONNECTED:
            ESP_LOGW(TAG, "❌ WiFi未连接");
            // 只在蓝牙已连接时发送状态
            if (ble_connected) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
            }
            break;
            
        case XN_WIFI_CONNECTING:
            ESP_LOGI(TAG, "🔄 WiFi连接中...");
            // 只在蓝牙已连接时发送状态
            if (ble_connected) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, 0, NULL);
            }
            break;
            
        case XN_WIFI_CONNECTED:
            ESP_LOGI(TAG, "📶 WiFi已连接");
            break;
            
        case XN_WIFI_GOT_IP: {
            ESP_LOGI(TAG, "✅ WiFi配网成功，已获取IP地址！");
            
            // 获取当前连接的WiFi配置
            wifi_config_t wifi_config;
            if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
                const char *ssid = (const char *)wifi_config.sta.ssid;
                const char *password = (const char *)wifi_config.sta.password;
                
                // 只在蓝牙已连接时发送状态
                if (ble_connected) {
                    // 发送连接成功状态（包含SSID）
                    esp_blufi_extra_info_t info = {0};
                    info.sta_ssid = wifi_config.sta.ssid;
                    info.sta_ssid_len = strlen(ssid);
                    esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
                    
                    ESP_LOGI(TAG, "📡 已发送WiFi状态到小程序: %s", ssid);
                }
                
                // 保存到NVS
                esp_err_t ret = xn_blufi_wifi_save(g_blufi, ssid, password);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "💾 WiFi配置已保存到NVS: %s", ssid);
                } else {
                    ESP_LOGE(TAG, "保存WiFi配置失败: %s", esp_err_to_name(ret));
                }
            }
            break;
        }
    }
}

/* 初始化蓝牙配网应用 */
esp_err_t app_blufi_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  蓝牙配网应用初始化");
    ESP_LOGI(TAG, "========================================");
    
    // 创建BluFi配网组件实例
    g_blufi = xn_blufi_create("ESP32_星年");
    if (g_blufi == NULL) {
        ESP_LOGE(TAG, "创建BluFi实例失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ BluFi实例创建成功");
    
    // 注册WiFi状态变化回调
    xn_blufi_wifi_register_status_cb(g_blufi, wifi_status_callback);
    ESP_LOGI(TAG, "✓ 状态回调已注册");
    
    // 初始化BluFi组件
    esp_err_t ret = xn_blufi_init(g_blufi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化失败: %s", esp_err_to_name(ret));
        xn_blufi_destroy(g_blufi);
        g_blufi = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "✓ BluFi初始化成功");
    
    // 尝试加载之前保存的WiFi配置
    xn_wifi_config_t config;
    if (xn_blufi_wifi_load(g_blufi, &config) == ESP_OK) {
        ESP_LOGI(TAG, "📱 发现保存的WiFi配置: %s", config.ssid);
        ESP_LOGI(TAG, "🔄 尝试自动连接...");
        xn_blufi_wifi_connect(g_blufi, config.ssid, config.password);
    } else {
        ESP_LOGI(TAG, "📱 未找到保存的WiFi配置");
        ESP_LOGI(TAG, "🔵 蓝牙广播已开启，等待小程序配网...");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "配网步骤：");
        ESP_LOGI(TAG, "  1. 打开微信小程序（搜索EspBlufi）");
        ESP_LOGI(TAG, "  2. 搜索并连接设备：ESP32_星年");
        ESP_LOGI(TAG, "  3. 输入WiFi名称和密码");
        ESP_LOGI(TAG, "  4. 点击配置按钮");
    }
    
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

/* 反初始化蓝牙配网应用 */
esp_err_t app_blufi_deinit(void)
{
    if (g_blufi == NULL) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "反初始化蓝牙配网应用");
    
    // 反初始化BluFi组件
    xn_blufi_deinit(g_blufi);
    
    // 销毁BluFi实例
    xn_blufi_destroy(g_blufi);
    g_blufi = NULL;
    
    ESP_LOGI(TAG, "蓝牙配网应用已关闭");
    
    return ESP_OK;
}
