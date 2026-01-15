/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: ESP32蓝牙小程序配网 By.星年
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_blufi.h"

static const char *TAG = "MAIN"; // 日志标签

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32蓝牙小程序配网 By.星年");
    ESP_LOGI(TAG, "========================================");
    
    // 初始化蓝牙配网应用层
    esp_err_t ret = app_blufi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "蓝牙配网应用初始化失败");
        return;
    }
    
    // 主循环 - 可以在这里添加其他业务逻辑
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1秒
        
        // TODO: 添加你的业务逻辑
        // 例如：连接MQTT服务器、上传数据等
    }
}
