# XN_BluFi 蓝牙配网组件

ESP32-S3蓝牙配网组件，通过蓝牙配置WiFi连接。使用NimBLE低功耗蓝牙协议栈。

## 功能特性

- ✅ 蓝牙接收WiFi配置（SSID、密码）
- ✅ WiFi连接、断开、自动重连
- ✅ 多WiFi配置保存到NVS（掉电不丢失，最多10个）
- ✅ WiFi扫描功能
- ✅ 面向对象设计，API简洁易用
- ✅ 使用NimBLE协议栈，低功耗
- ✅ 模块化三层架构设计

⚠️ **安全提示**：本组件使用明文传输，仅适用于内网环境。

## 快速开始

### 1. 添加组件到项目

组件已在`components/xn_blufi`目录下。

### 2. 配置sdkconfig

项目已包含`sdkconfig.defaults`，自动配置NimBLE。如需手动配置：

```bash
idf.py menuconfig
# Component config -> Bluetooth -> [*] Bluetooth
# Component config -> Bluetooth -> Bluetooth Host -> NimBLE
# Component config -> Bluetooth -> NimBLE Options -> [*] Enable blufi
```

### 3. 基础使用

```c
#include "app_blufi.h"

void app_main(void)
{
    // 初始化蓝牙配网应用
    app_blufi_init();
    
    // 等待配网...
}
```

## 配网流程

1. ESP32-S3启动并开启蓝牙广播
2. 客户端搜索并连接设备
3. 客户端发送WiFi配置信息
4. ESP32-S3连接WiFi
5. 配网完成

## API参考

详见`include/xn_blufi.h`头文件注释。

## 技术支持

- 作者：星年
- 邮箱：jixingnian@gmail.com
- 基于：ESP-IDF BluFi示例
