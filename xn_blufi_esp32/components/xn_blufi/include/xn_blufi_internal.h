/*
 * @Author: 星年 jixingnian@gmail.com
 * @Date: 2025-01-14
 * @Description: BluFi内部头文件 - NimBLE相关函数声明
 */

#ifndef XN_BLUFI_INTERNAL_H
#define XN_BLUFI_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NimBLE重置回调 */
void xn_blufi_on_reset(int reason);

/* NimBLE同步回调 */
void xn_blufi_on_sync(void);

/* NimBLE主机任务 */
void xn_blufi_host_task(void *param);

#ifdef __cplusplus
}
#endif

#endif // XN_BLUFI_INTERNAL_H
