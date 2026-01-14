/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: 蓝牙配网应用层 - 实现文件
 */

#include "app_blufi.h"
#include "xn_blufi.h"
#include "esp_log.h"

static const char *TAG = "APP_BLUFI"; // 日志标签
static xn_blufi_t *g_blufi = NULL;    // BluFi实例

/* WiFi状态变化回调函数 */
static void wifi_status_callback(xn_wifi_status_t status)
{
    switch(status) {
        case XN_WIFI_DISCONNECTED:
            ESP_LOGW(TAG, "❌ WiFi未连接");
            break;
            
        case XN_WIFI_CONNECTING:
            ESP_LOGI(TAG, "🔄 WiFi连接中...");
            break;
            
        case XN_WIFI_CONNECTED:
            ESP_LOGI(TAG, "📶 WiFi已连接");
            break;
            
        case XN_WIFI_GOT_IP:
            ESP_LOGI(TAG, "✅ WiFi配网成功，已获取IP地址！");
            // 配网成功后保存配置到NVS
            xn_wifi_config_t config;
            if (xn_blufi_wifi_load(g_blufi, &config) == ESP_OK) {
                xn_blufi_wifi_save(g_blufi, config.ssid, config.password);
                ESP_LOGI(TAG, "WiFi配置已保存到NVS");
            }
            break;
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
