/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: 蓝牙配网应用层 - 头文件
 */

#ifndef APP_BLUFI_H
#define APP_BLUFI_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蓝牙配网应用
 * @return ESP_OK成功，其他值失败
 */
esp_err_t app_blufi_init(void);

/**
 * @brief 反初始化蓝牙配网应用
 * @return ESP_OK成功，其他值失败
 */
esp_err_t app_blufi_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // APP_BLUFI_H
