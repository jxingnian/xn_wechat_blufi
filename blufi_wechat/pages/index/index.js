// index.js - BluFi 配网页面
const BluFiProtocol = require('../../utils/blufi.js')

Page({
  data: {
    // 蓝牙状态
    bluetoothEnabled: false,
    
    // 扫描状态
    scanning: false,
    devices: [],
    
    // 连接状态
    connected: false,
    connectedDevice: null,
    
    // 设备WiFi状态
    deviceWifiStatus: null,  // {connected, ssid, password, opmode, ...}
    
    // 存储的WiFi配置
    storedConfig: null,  // {exists, ssid, password}
    
    // WiFi列表
    wifiList: [],
    selectedWifi: null,
    wifiPassword: '',
    
    // 配网状态
    configuring: false,
    configResult: '',
    
    // 当前视图 (scan:扫描设备, status:设备状态管理)
    currentView: 'scan'
  },

  onLoad() {
    // 初始化 BluFi 协议实例
    this.blufi = new BluFiProtocol()
    
    // 注册回调
    this.blufi.on('onWifiList', (wifiList) => {
      console.log('收到WiFi列表:', wifiList)
      this.setData({ wifiList: wifiList })
      wx.hideLoading()
    })
    
    this.blufi.on('onWifiStatus', (status) => {
      console.log('收到WiFi状态:', status)
      this.setData({ 
        deviceWifiStatus: status,
        configuring: false
      })
      wx.hideLoading()
      
      // 如果正在配网，显示结果
      if (this.data.configuring) {
        if (status.connected) {
          this.setData({ configResult: 'success' })
          wx.showToast({ title: '配网成功', icon: 'success' })
        } else {
          this.setData({ configResult: 'fail' })
          wx.showToast({ title: '配网失败', icon: 'none' })
        }
      }
    })
    
    this.blufi.on('onStoredConfig', (data) => {
      console.log('收到存储的WiFi配置:', data)
      this.setData({ storedConfig: data })
      wx.hideLoading()
    })
    
    this.blufi.on('onConfigDeleted', () => {
      console.log('配置已删除，刷新列表')
      // 删除成功后重新获取配置列表
      setTimeout(() => {
        this.getStoredConfig()
      }, 500)
    })
    
    this.blufi.on('onError', (errorCode) => {
      console.error('BluFi错误:', errorCode)
      wx.hideLoading()
      wx.showToast({ title: '操作失败', icon: 'none' })
    })
    
    // 监听蓝牙断开
    this.blufi.on('onDisconnected', () => {
      console.log('蓝牙连接已断开')
      
      // 如果当前是已连接状态，显示断开提示
      if (this.data.connected) {
        wx.showToast({ 
          title: '蓝牙已断开', 
          icon: 'none',
          duration: 2000
        })
        
        // 重置状态
        this.setData({
          connected: false,
          connectedDevice: null,
          deviceWifiStatus: null,
          wifiList: [],
          selectedWifi: null,
          wifiPassword: '',
          configResult: null,
          currentView: 'scan'
        })
        
        // 重新开始扫描
        setTimeout(() => {
          this.startScan()
        }, 1000)
      }
    })
    
    this.checkBluetoothStatus()
  },

  // ========== 蓝牙管理 ==========
  checkBluetoothStatus() {
    wx.openBluetoothAdapter({
      success: () => {
        console.log('蓝牙适配器已打开')
        this.setData({ bluetoothEnabled: true })
        // 自动开始扫描
        this.startScan()
      },
      fail: (err) => {
        console.error('打开蓝牙适配器失败:', err)
        wx.showModal({
          title: '提示',
          content: '请先打开手机蓝牙',
          confirmText: '去设置',
          success: (res) => {
            if (res.confirm) {
              wx.openSetting()
            }
          }
        })
      }
    })
  },

  openBluetooth() {
    this.checkBluetoothStatus()
  },

  // ========== 设备扫描 ==========
  startScan() {
    if (this.data.scanning) return
    
    this.setData({ 
      scanning: true,
      devices: []
    })

    wx.showLoading({ title: '扫描中...' })

    wx.startBluetoothDevicesDiscovery({
      allowDuplicatesKey: false,
      success: () => {
        console.log('开始扫描蓝牙设备')
        
        wx.onBluetoothDeviceFound((res) => {
          res.devices.forEach((device) => {
            const name = device.name || device.localName
            
            // 只添加ESP32开头的设备，提高扫描效率
            if (name && name.toUpperCase().startsWith('ESP32')) {
              const devices = this.data.devices
              const index = devices.findIndex(d => d.deviceId === device.deviceId)
              
              if (index === -1) {
                device.name = name
                devices.push(device)
                this.setData({ devices })
                console.log('发现ESP32设备:', name, device.deviceId)
              }
            }
          })
        })

        // 5秒后停止扫描（ESP32设备通常很快就能发现）
        setTimeout(() => {
          this.stopScan()
        }, 5000)
      },
      fail: (err) => {
        console.error('扫描失败:', err)
        wx.hideLoading()
        wx.showToast({ title: '扫描失败', icon: 'none' })
        this.setData({ scanning: false })
      }
    })
  },

  stopScan() {
    wx.stopBluetoothDevicesDiscovery()
    wx.hideLoading()
    this.setData({ scanning: false })
    
    if (this.data.devices.length === 0) {
      wx.showToast({ title: '未发现设备', icon: 'none' })
    }
  },

  rescan() {
    this.startScan()
  },

  // ========== 设备连接 ==========
  connectDevice(e) {
    const device = e.currentTarget.dataset.device
    
    wx.showLoading({ title: '连接中...' })

    this.blufi.connect(device.deviceId)
      .then(() => {
        console.log('BluFi连接成功:', device.name)
        this.setData({
          connected: true,
          connectedDevice: device,
          currentView: 'status'
        })
        wx.hideLoading()
        wx.showToast({ title: '连接成功', icon: 'success' })
        
        // 自动获取设备WiFi状态
        this.refreshDeviceStatus()
      })
      .catch((err) => {
        console.error('连接失败:', err)
        wx.hideLoading()
        wx.showToast({ title: '连接失败', icon: 'none' })
      })
  },

  disconnect() {
    if (this.data.connectedDevice) {
      this.blufi.disconnect()
        .then(() => {
          this.setData({
            connected: false,
            connectedDevice: null,
            deviceWifiStatus: null,
            wifiList: [],
            currentView: 'scan'
          })
          wx.showToast({ title: '已断开连接', icon: 'success' })
        })
        .catch((err) => {
          console.error('断开连接失败:', err)
        })
    }
  },

  // ========== 设备状态管理 ==========
  refreshDeviceStatus() {
    wx.showLoading({ title: '获取状态...' })
    
    // 同时获取WiFi状态和存储的配置
    Promise.all([
      this.blufi.requestWifiStatus(),
      this.blufi.requestStoredConfig()
    ]).catch((err) => {
      console.error('获取状态失败:', err)
      wx.hideLoading()
    })
  },

  // 获取存储的WiFi配置
  getStoredConfig() {
    wx.showLoading({ title: '获取配置...' })
    this.blufi.requestStoredConfig()
      .catch((err) => {
        console.error('获取配置失败:', err)
        wx.hideLoading()
      })
  },

  // 删除存储的WiFi配置
  deleteStoredConfig(e) {
    const index = e.currentTarget.dataset.index
    const ssid = e.currentTarget.dataset.ssid
    
    wx.showModal({
      title: '确认',
      content: `确定要删除配置"${ssid}"吗？`,
      success: (res) => {
        if (res.confirm) {
          wx.showLoading({ title: '删除中...' })
          this.blufi.deleteStoredConfig(index)
            .catch((err) => {
              console.error('删除失败:', err)
              wx.hideLoading()
            })
        }
      }
    })
  },

  // 使用存储的配置连接
  connectWithStoredConfig(e) {
    const config = e.currentTarget.dataset.config

    wx.showModal({
      title: '确认',
      content: `使用配置"${config.ssid}"连接WiFi？`,
      success: (res) => {
        if (res.confirm) {
          this.setData({ 
            configuring: true,
            configResult: ''
          })
          wx.showLoading({ title: '连接中...' })

          this.blufi.sendWifiConfig(config.ssid, config.password)
            .then(() => {
              setTimeout(() => {
                this.refreshDeviceStatus()
              }, 3000)
            })
            .catch((err) => {
              console.error('连接失败:', err)
              this.setData({ configuring: false })
              wx.hideLoading()
            })
        }
      }
    })
  },

  // 扫描WiFi
  scanWifi() {
    wx.showLoading({ title: '扫描WiFi...' })
    this.blufi.requestWifiList()
      .catch((err) => {
        console.error('扫描失败:', err)
        wx.hideLoading()
      })
  },

  // 选择WiFi
  selectWifi(e) {
    const wifi = e.currentTarget.dataset.wifi
    this.setData({ 
      selectedWifi: wifi,
      wifiPassword: ''
    })
  },

  // 输入密码
  onPasswordInput(e) {
    this.setData({ wifiPassword: e.detail.value })
  },

  // 配置WiFi
  configWifi() {
    if (!this.data.selectedWifi) {
      wx.showToast({ title: '请选择WiFi', icon: 'none' })
      return
    }

    if (this.data.selectedWifi.secure && !this.data.wifiPassword) {
      wx.showToast({ title: '请输入密码', icon: 'none' })
      return
    }

    this.setData({ 
      configuring: true,
      configResult: ''
    })
    wx.showLoading({ title: '配置中...' })

    this.blufi.sendWifiConfig(this.data.selectedWifi.ssid, this.data.wifiPassword)
      .then(() => {
        console.log('WiFi配置已发送')
        // 等待3秒后查询状态
        setTimeout(() => {
          this.refreshDeviceStatus()
        }, 3000)
      })
      .catch((err) => {
        console.error('配置失败:', err)
        this.setData({ configuring: false })
        wx.hideLoading()
        wx.showToast({ title: '配置失败', icon: 'none' })
      })
  },

  // 断开设备WiFi
  disconnectDeviceWifi() {
    wx.showModal({
      title: '确认',
      content: '确定要断开设备的WiFi连接吗？',
      success: (res) => {
        if (res.confirm) {
          wx.showLoading({ title: '断开中...' })
          this.blufi.disconnectWifi()
            .then(() => {
              setTimeout(() => {
                this.refreshDeviceStatus()
              }, 1000)
            })
            .catch((err) => {
              console.error('断开失败:', err)
              wx.hideLoading()
            })
        }
      }
    })
  },

  // 切换视图
  switchView(e) {
    const view = e.currentTarget.dataset.view
    this.setData({ currentView: view })
    
    if (view === 'scan') {
      this.startScan()
    }
  },

  // 页面卸载时清理资源
  onUnload() {
    console.log('页面卸载，清理蓝牙连接')
    
    // 断开蓝牙连接
    if (this.blufi && this.data.connected) {
      this.blufi.disconnect().catch(err => {
        console.error('断开连接失败:', err)
      })
    }
    
    // 关闭蓝牙适配器
    wx.closeBluetoothAdapter({
      success: () => {
        console.log('蓝牙适配器已关闭')
      }
    })
  }
})
