# ESP32-S3 蓝牙配网项目

通过蓝牙实现ESP32-S3的WiFi配网功能。

## 项目结构

```
xn_blefi_esp32/
├── components/
│   └── xn_blufi/              # BluFi配网组件
│       ├── include/
│       │   ├── xn_blufi.h           # 公共API头文件
│       │   └── xn_blufi_internal.h  # 内部头文件
│       ├── xn_blufi.c               # 主要实现
│       ├── xn_blufi_security.c      # 安全层实现
│       ├── xn_blufi_gatt.c          # GATT服务器实现
│       ├── CMakeLists.txt           # 组件构建配置
│       └── README.md                # 组件使用文档
├── main/
│   ├── main.c                 # 主程序
│   ├── app_blufi.c            # 蓝牙配网应用层
│   ├── app_blufi.h            # 应用层头文件
│   └── CMakeLists.txt
├── sdkconfig.defaults         # SDK默认配置
└── README.md                  # 本文件
```

## 快速开始

### 1. 环境准备

- ESP-IDF v5.5
- ESP32-S3开发板
- USB数据线

### 2. 编译项目

```bash
# 设置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录
idf.py -p COM3 flash

# 查看日志
idf.py -p COM3 monitor
```

### 3. 配网方式

使用支持BluFi协议的客户端进行配网（客户端开发中）。

### 4. 查看运行日志

```
========================================
  ESP32蓝牙小程序配网 By.星年
========================================
========================================
  蓝牙配网应用初始化
========================================
✓ BluFi实例创建成功
✓ 状态回调已注册
✓ BluFi初始化成功
📱 未找到保存的WiFi配置
🔵 蓝牙广播已开启，等待配网...
========================================
```

## 组件功能

### 核心功能

- ✅ 蓝牙接收WiFi配置
- ✅ WiFi自动连接和重连
- ✅ WiFi配置持久化存储（NVS）
- ✅ WiFi扫描功能
- ✅ 安全加密传输（DH密钥交换 + AES加密）
- ✅ 使用NimBLE低功耗蓝牙协议栈
- ✅ 状态回调通知

### API列表

详细API文档请查看：[components/xn_blufi/README.md](components/xn_blufi/README.md)

## 自定义配置

### 修改蓝牙设备名称

在`main/app_blufi.c`中修改：

```c
g_blufi = xn_blufi_create("你的设备名");
```

## 许可证

本项目基于ESP-IDF示例代码开发，遵循Apache 2.0许可证。

## 作者

星年 (jixingnian@gmail.com)

## 更新日志

### v1.0.0 (2025-01-14)
- ✨ 初始版本发布
- ✅ 完整的BluFi配网功能
- ✅ WiFi管理API
- ✅ NVS存储支持
- ✅ 使用NimBLE低功耗协议栈
- ✅ 详细的代码注释
