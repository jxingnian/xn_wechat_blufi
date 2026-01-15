/**
 * BluFi 协议处理模块
 * 基于 ESP-IDF BluFi 协议实现
 * 
 * ⚠️ 注意：本实现使用明文传输，仅适用于内网环境
 */

// BluFi 服务和特征值 UUID
const BLUFI_SERVICE_UUID = '0000FFFF-0000-1000-8000-00805F9B34FB'
const BLUFI_WRITE_UUID = '0000FF01-0000-1000-8000-00805F9B34FB'
const BLUFI_NOTIFY_UUID = '0000FF02-0000-1000-8000-00805F9B34FB'

// BluFi 帧类型
const BLUFI_TYPE_CTRL = 0x00
const BLUFI_TYPE_DATA = 0x01

// BluFi 帧控制标志
const BLUFI_FC_ENC = 0x01        // 加密
const BLUFI_FC_CHECK = 0x02      // 校验
const BLUFI_FC_DIR = 0x04        // 方向
const BLUFI_FC_REQ_ACK = 0x08    // 请求确认
const BLUFI_FC_FRAG = 0x10       // 分片

// BluFi 控制子类型（根据 ESP-IDF 官方定义）
const BLUFI_CTRL_SUBTYPE_ACK = 0x00
const BLUFI_CTRL_SUBTYPE_SET_SEC_MODE = 0x01
const BLUFI_CTRL_SUBTYPE_SET_OPMODE = 0x02
const BLUFI_CTRL_SUBTYPE_CONNECT_WIFI = 0x03
const BLUFI_CTRL_SUBTYPE_DISCONNECT_WIFI = 0x04
const BLUFI_CTRL_SUBTYPE_GET_WIFI_STATUS = 0x05
const BLUFI_CTRL_SUBTYPE_DEAUTHENTICATE = 0x06
const BLUFI_CTRL_SUBTYPE_GET_VERSION = 0x07
const BLUFI_CTRL_SUBTYPE_DISCONNECT_BLE = 0x08
const BLUFI_CTRL_SUBTYPE_GET_WIFI_LIST = 0x09

// BluFi 数据子类型
const BLUFI_DATA_SUBTYPE_NEG = 0x00
const BLUFI_DATA_SUBTYPE_STA_BSSID = 0x01
const BLUFI_DATA_SUBTYPE_STA_SSID = 0x02
const BLUFI_DATA_SUBTYPE_STA_PASSWD = 0x03
const BLUFI_DATA_SUBTYPE_SOFTAP_SSID = 0x04
const BLUFI_DATA_SUBTYPE_SOFTAP_PASSWD = 0x05
const BLUFI_DATA_SUBTYPE_SOFTAP_MAX_CONN = 0x06
const BLUFI_DATA_SUBTYPE_SOFTAP_AUTH_MODE = 0x07
const BLUFI_DATA_SUBTYPE_SOFTAP_CHANNEL = 0x08
const BLUFI_DATA_SUBTYPE_USERNAME = 0x09
const BLUFI_DATA_SUBTYPE_CA = 0x0a
const BLUFI_DATA_SUBTYPE_CLIENT_CERT = 0x0b
const BLUFI_DATA_SUBTYPE_SERVER_CERT = 0x0c
const BLUFI_DATA_SUBTYPE_CLIENT_PRIV_KEY = 0x0d
const BLUFI_DATA_SUBTYPE_SERVER_PRIV_KEY = 0x0e
const BLUFI_DATA_SUBTYPE_WIFI_REP = 0x0f
const BLUFI_DATA_SUBTYPE_REPLY_VERSION = 0x10
const BLUFI_DATA_SUBTYPE_WIFI_LIST = 0x11
const BLUFI_DATA_SUBTYPE_ERROR = 0x12
const BLUFI_DATA_SUBTYPE_CUSTOM_DATA = 0x13

class BluFiProtocol {
  constructor() {
    this.deviceId = null
    this.serviceId = BLUFI_SERVICE_UUID
    this.writeCharId = BLUFI_WRITE_UUID
    this.notifyCharId = BLUFI_NOTIFY_UUID
    this.sequence = 0
    this.receiveBuffer = []
    this.callbacks = {}
    
    // 分包重组缓冲区
    this.fragmentBuffer = null
    this.fragmentExpectedLength = 0
  }

  // 连接设备
  connect(deviceId) {
    return new Promise((resolve, reject) => {
      this.deviceId = deviceId
      
      // 重置序列号和状态（重要！）
      this.sequence = 0
      this.receiveBuffer = []
      this.fragmentBuffer = null
      this.fragmentExpectedLength = 0
      console.log('✓ 序列号已重置为0')
      
      // 监听蓝牙连接状态变化
      wx.onBLEConnectionStateChange((res) => {
        console.log('蓝牙连接状态变化:', res)
        if (res.deviceId === this.deviceId) {
          if (!res.connected) {
            console.log('⚠️ 蓝牙连接已断开')
            // 触发断开回调
            if (this.callbacks.onDisconnected) {
              this.callbacks.onDisconnected()
            }
          }
        }
      })
      
      wx.createBLEConnection({
        deviceId: deviceId,
        success: () => {
          console.log('BLE连接成功')
          
          // 尝试设置更大的MTU（仅安卓有效）
          wx.setBLEMTU({
            deviceId: deviceId,
            mtu: 512,
            success: (res) => {
              console.log('✓ MTU已设置为:', res.mtu)
            },
            fail: (err) => {
              console.log('MTU设置失败（iOS不支持）:', err.errMsg)
            }
          })
          
          // 延迟获取服务，确保连接稳定
          setTimeout(() => {
            this.discoverServices().then(resolve).catch(reject)
          }, 1000)
        },
        fail: reject
      })
    })
  }

  // 发现服务
  discoverServices() {
    return new Promise((resolve, reject) => {
      wx.getBLEDeviceServices({
        deviceId: this.deviceId,
        success: (res) => {
          console.log('获取到服务:', res.services)
          
          // 查找 BluFi 服务
          const service = res.services.find(s => 
            s.uuid.toUpperCase().includes('FFFF')
          )
          
          if (service) {
            this.serviceId = service.uuid
            this.discoverCharacteristics().then(resolve).catch(reject)
          } else {
            reject(new Error('未找到BluFi服务'))
          }
        },
        fail: reject
      })
    })
  }

  // 发现特征值
  discoverCharacteristics() {
    return new Promise((resolve, reject) => {
      wx.getBLEDeviceCharacteristics({
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        success: (res) => {
          console.log('获取到特征值:', res.characteristics)
          
          // 查找写特征值和通知特征值
          res.characteristics.forEach(char => {
            const uuid = char.uuid.toUpperCase()
            if (uuid.includes('FF01')) {
              this.writeCharId = char.uuid
            } else if (uuid.includes('FF02')) {
              this.notifyCharId = char.uuid
            }
          })
          
          // 启用通知
          this.enableNotify().then(resolve).catch(reject)
        },
        fail: reject
      })
    })
  }

  // 启用通知
  enableNotify() {
    return new Promise((resolve, reject) => {
      // 监听特征值变化
      wx.onBLECharacteristicValueChange((res) => {
        this.handleNotify(res.value)
      })
      
      // 启用通知
      wx.notifyBLECharacteristicValueChange({
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        characteristicId: this.notifyCharId,
        state: true,
        success: () => {
          console.log('通知已启用')
          // 暂时禁用加密，直接完成连接
          console.log('⚠️ 加密已禁用（调试模式）')
          
          // 延迟500ms，确保ESP32端完全准备好
          // 避免序列号不同步问题
          setTimeout(() => {
            console.log('✓ BluFi连接完全建立，可以开始通信')
            resolve()
          }, 500)
        },
        fail: reject
      })
    })
  }

  // 开始密钥协商
  // 处理通知数据
  handleNotify(buffer) {
    const data = new Uint8Array(buffer)
    console.log('>>> 收到通知，长度:', data.length, '数据:', Array.from(data).slice(0, 20))
    
    // 如果有未完成的分包，继续拼接
    if (this.fragmentBuffer) {
      console.log('>>> 拼接分包数据，原长度:', this.fragmentBuffer.length, '新数据:', data.length)
      const combined = new Uint8Array(this.fragmentBuffer.length + data.length)
      combined.set(this.fragmentBuffer)
      combined.set(data, this.fragmentBuffer.length)
      this.fragmentBuffer = combined
      
      console.log('>>> 当前缓冲区长度:', this.fragmentBuffer.length, '期望长度:', this.fragmentExpectedLength)
      
      // 检查是否已接收完整
      if (this.fragmentBuffer.length >= this.fragmentExpectedLength) {
        console.log('✓ 分包接收完成，开始解析')
        const frame = this.parseFrame(this.fragmentBuffer)
        this.fragmentBuffer = null
        this.fragmentExpectedLength = 0
        
        if (frame) {
          this.handleFrame(frame)
        }
      } else {
        console.log('>>> 继续等待分包，还需要:', this.fragmentExpectedLength - this.fragmentBuffer.length, '字节')
      }
      return
    }
    
    // 解析 BluFi 帧
    const frame = this.parseFrame(data)
    if (frame) {
      this.handleFrame(frame)
    } else if (!this.fragmentBuffer) {
      console.warn('帧解析失败且未启动分包缓冲')
    }
  }

  // 解析帧
  parseFrame(data) {
    if (data.length < 4) return null
    
    const type = data[0] & 0x03
    const subtype = (data[0] >> 2) & 0x3F
    const fc = data[1]
    const sequence = data[2]
    const dataLen = data[3]
    
    console.log('解析帧:', {
      type: type,
      subtype: subtype,
      fc: fc,
      sequence: sequence,
      dataLen: dataLen,
      totalLen: data.length
    })
    
    const expectedLen = 4 + dataLen + (fc & BLUFI_FC_CHECK ? 2 : 0)
    
    if (data.length < expectedLen) {
      console.warn('帧数据不完整，需要:', expectedLen, '当前:', data.length)
      // 保存到分包缓冲区
      this.fragmentBuffer = new Uint8Array(data)
      this.fragmentExpectedLength = expectedLen
      console.log('等待后续分包...')
      return null
    }
    
    let payload = data.slice(4, 4 + dataLen)
    
    console.log('Payload原始数据（前20字节）:', Array.from(payload.slice(0, 20)))
    
    return {
      type: type,
      subtype: subtype,
      fc: fc,
      sequence: sequence,
      dataLen: dataLen,
      payload: payload
    }
  }

  // 处理帧
  handleFrame(frame) {
    console.log('处理帧:', frame)
    
    if (frame.type === BLUFI_TYPE_DATA) {
      switch (frame.subtype) {
        case BLUFI_DATA_SUBTYPE_WIFI_LIST:
          this.handleWifiList(frame.payload)
          break
        case BLUFI_DATA_SUBTYPE_WIFI_REP:
          this.handleWifiStatus(frame.payload)
          break
        case BLUFI_DATA_SUBTYPE_CUSTOM_DATA:
          this.handleCustomData(frame.payload)
          break
        case BLUFI_DATA_SUBTYPE_ERROR:
          this.handleError(frame.payload)
          break
      }
    }
  }

  // 处理密钥协商
  // 处理WiFi列表
  handleWifiList(payload) {
    console.log('=== 开始解析WiFi列表 ===')
    console.log('Payload长度:', payload.length)
    
    const wifiList = []
    let offset = 0
    
    while (offset < payload.length) {
      // ESP-IDF BluFi 格式：[ssid_len, rssi, ssid_bytes...]
      // ssid_len 包含了NULL字符，所以实际SSID长度是 ssid_len - 1
      
      if (offset + 2 > payload.length) {
        console.warn('数据不完整')
        break
      }
      
      console.log(`\n--- WiFi #${wifiList.length + 1} ---`)
      console.log(`当前offset=${offset}`)
      
      const ssidLenRaw = payload[offset++]
      const rssi = payload[offset++]
      
      // SSID长度减1（去掉NULL字符）
      const ssidLen = ssidLenRaw > 0 ? ssidLenRaw - 1 : 0
      
      console.log(`原始长度=${ssidLenRaw}, 实际SSID长度=${ssidLen}, RSSI=${rssi}`)
      console.log(`将读取 [${offset}, ${offset + ssidLen})`)
      
      if (ssidLen === 0 || ssidLen > 32 || offset + ssidLen > payload.length) {
        console.warn(`SSID长度异常`)
        break
      }
      
      // 读取 ssidLen 个字节
      const ssidBytes = payload.slice(offset, offset + ssidLen)
      // 只跳过实际SSID字节数（ESP-IDF虽然在长度上+1，但实际没发送NULL）
      offset += ssidLen
      
      const rssiValue = rssi > 127 ? rssi - 256 : -rssi
      
      // 将字节数组转换为字符串
      let ssid = ''
      for (let i = 0; i < ssidBytes.length; i++) {
        ssid += String.fromCharCode(ssidBytes[i])
      }
      
      console.log(`✓ SSID: "${ssid}", RSSI: ${rssiValue}dBm, 新offset=${offset}`)
      
      wifiList.push({
        ssid: ssid,
        rssi: rssiValue,
        secure: true
      })
    }
    
    console.log('\n=== WiFi列表解析完成 ===')
    console.log('共解析到', wifiList.length, '个WiFi')
    
    if (this.callbacks.onWifiList) {
      this.callbacks.onWifiList(wifiList)
    }
  }

  // 处理WiFi状态
  handleWifiStatus(payload) {
    console.log('=== 收到WiFi状态报告 ===')
    console.log('Payload长度:', payload.length)
    console.log('Payload数据:', Array.from(payload))
    
    if (payload.length < 3) {
      console.warn('WiFi状态数据不完整')
      return
    }
    
    const opmode = payload[0]  // 操作模式
    const sta_conn_state = payload[1]  // STA连接状态
    const softap_conn_num = payload[2]  // AP连接数
    
    // 解析状态
    const status = {
      opmode: opmode,  // 1=STA, 2=AP, 3=STA+AP
      connected: sta_conn_state === 0,  // 0=成功, 其他=失败
      sta_conn_state: sta_conn_state,
      softap_conn_num: softap_conn_num,
      ssid: '',
      password: '',
      bssid: ''
    }
    
    // 解析额外信息（如果有）
    // 根据ESP-IDF BluFi协议文档，额外信息使用Data Frame的子类型编码：
    // 0x01 = STA BSSID
    // 0x02 = STA SSID  
    // 0x03 = STA Password
    // 0x04 = SoftAP SSID
    // 0x05 = SoftAP Password
    let offset = 3
    while (offset < payload.length) {
      const type = payload[offset++]
      const len = payload[offset++]
      
      console.log(`  额外信息 - type: ${type}, len: ${len}, offset: ${offset}`)
      
      if (offset + len > payload.length) {
        console.warn('额外信息长度超出范围')
        break
      }
      
      const data = payload.slice(offset, offset + len)
      offset += len
      
      switch(type) {
        case 0x01: // STA BSSID
          status.bssid = Array.from(data).map(b => b.toString(16).padStart(2, '0')).join(':')
          console.log('  解析到BSSID:', status.bssid)
          break
        case 0x02: // STA SSID
          status.ssid = this.bytesToString(data)
          console.log('  解析到SSID:', status.ssid)
          break
        case 0x03: // STA Password
          status.password = this.bytesToString(data)
          console.log('  解析到Password')
          break
        case 0x04: // SoftAP SSID
          console.log('  SoftAP SSID (忽略)')
          break
        case 0x05: // SoftAP Password
          console.log('  SoftAP Password (忽略)')
          break
        default:
          console.warn('  未知的额外信息类型:', type)
          break
      }
    }
    
    console.log('解析后的状态:', status)
    
    if (this.callbacks.onWifiStatus) {
      this.callbacks.onWifiStatus(status)
    }
  }

  // 处理错误
  handleError(payload) {
    const errorCode = payload[0]
    
    // 错误码定义（根据ESP-IDF BluFi协议）
    const errorMessages = {
      0x00: '序列号错误',
      0x01: '校验和错误',
      0x02: '解密错误',
      0x03: '加密错误',
      0x04: '初始化安全错误',
      0x05: 'DH内存分配错误',
      0x06: 'DH参数错误',
      0x07: '读取参数错误',
      0x08: '生成公钥错误'
    }
    
    const errorMsg = errorMessages[errorCode] || `未知错误(${errorCode})`
    console.error('BluFi错误:', errorMsg)
    
    // 如果是序列号错误，重置序列号
    if (errorCode === 0x00) {
      console.warn('⚠️ 检测到序列号错误，重置序列号为0')
      this.sequence = 0
      // 不触发onError回调，因为已经自动修复
      return
    }
    
    if (this.callbacks.onError) {
      this.callbacks.onError(errorCode)
    }
  }

  // 处理自定义数据
  handleCustomData(payload) {
    console.log('=== 收到自定义数据 ===')
    console.log('Payload长度:', payload.length)
    
    if (payload.length < 2) {
      console.warn('自定义数据太短')
      return
    }
    
    const type = payload[0]
    const status = payload[1]
    
    // 类型 0x01: 存储的WiFi配置（多个）
    if (type === 0x01) {
      if (status === 0x00 && payload.length > 2) {
        const count = payload[2]
        console.log('存储的WiFi配置数量:', count)
        
        const configs = []
        let offset = 3
        
        for (let i = 0; i < count && offset < payload.length; i++) {
          // SSID
          const ssidLen = payload[offset++]
          if (offset + ssidLen > payload.length) break
          
          const ssidBytes = payload.slice(offset, offset + ssidLen)
          offset += ssidLen
          const ssid = this.bytesToString(ssidBytes)
          
          // 密码
          if (offset >= payload.length) break
          const pwdLen = payload[offset++]
          if (offset + pwdLen > payload.length) break
          
          const pwdBytes = payload.slice(offset, offset + pwdLen)
          offset += pwdLen
          const password = this.bytesToString(pwdBytes)
          
          configs.push({
            index: i,
            ssid: ssid,
            password: password
          })
          
          console.log(`  [${i}] ${ssid}`)
        }
        
        console.log('解析完成，共', configs.length, '个配置')
        
        if (this.callbacks.onStoredConfig) {
          this.callbacks.onStoredConfig({
            exists: configs.length > 0,
            configs: configs
          })
        }
      } else {
        // 未找到存储的配置
        console.log('设备未存储WiFi配置')
        if (this.callbacks.onStoredConfig) {
          this.callbacks.onStoredConfig({ 
            exists: false,
            configs: []
          })
        }
      }
    }
    // 类型 0x02: 删除配置响应
    else if (type === 0x02) {
      if (status === 0x00) {
        console.log('✓ 配置删除成功')
        // 删除成功后重新获取配置列表
        if (this.callbacks.onConfigDeleted) {
          this.callbacks.onConfigDeleted()
        }
      } else {
        console.error('✗ 配置删除失败')
      }
    }
  }

  // 构建帧
  buildFrame(type, subtype, payload = []) {
    let fc = 0
    let actualPayload = payload
    
    const frameLen = 4 + actualPayload.length
    const frame = new Uint8Array(frameLen)
    
    frame[0] = (subtype << 2) | type
    frame[1] = fc
    frame[2] = this.sequence  // 先使用当前序列号
    frame[3] = actualPayload.length
    
    if (actualPayload.length > 0) {
      frame.set(actualPayload, 4)
    }
    
    this.sequence++  // 发送后再自增
    
    return frame.buffer
  }

  // 发送数据
  sendData(buffer) {
    return new Promise((resolve, reject) => {
      console.log('发送数据到设备:', {
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        characteristicId: this.writeCharId,
        dataLength: buffer.byteLength
      })
      
      wx.writeBLECharacteristicValue({
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        characteristicId: this.writeCharId,
        value: buffer,
        success: (res) => {
          console.log('数据发送成功:', res)
          resolve(res)
        },
        fail: (err) => {
          console.error('数据发送失败:', err)
          reject(err)
        }
      })
    })
  }

  // 请求WiFi列表
  requestWifiList() {
    console.log('=== 开始请求WiFi列表 ===')
    const frame = this.buildFrame(BLUFI_TYPE_CTRL, BLUFI_CTRL_SUBTYPE_GET_WIFI_LIST)
    const frameArray = Array.from(new Uint8Array(frame))
    console.log('发送帧:', frameArray)
    console.log('帧解析: type=' + (frameArray[0] & 0x03) + ', subtype=' + ((frameArray[0] >> 2) & 0x3F) + ', fc=' + frameArray[1] + ', seq=' + frameArray[2] + ', len=' + frameArray[3])
    return this.sendData(frame)
  }

  // 请求WiFi状态
  requestWifiStatus() {
    console.log('=== 请求WiFi状态 ===')
    const frame = this.buildFrame(BLUFI_TYPE_CTRL, BLUFI_CTRL_SUBTYPE_GET_WIFI_STATUS)
    return this.sendData(frame)
  }

  // 断开WiFi连接
  disconnectWifi() {
    console.log('=== 请求断开WiFi ===')
    const frame = this.buildFrame(BLUFI_TYPE_CTRL, BLUFI_CTRL_SUBTYPE_DISCONNECT_WIFI)
    return this.sendData(frame)
  }

  // 请求存储的WiFi配置
  requestStoredConfig() {
    console.log('=== 请求存储的WiFi配置 ===')
    // 发送自定义数据请求：类型 0x01 = 获取存储的WiFi配置
    const customData = [0x01]
    const frame = this.buildFrame(BLUFI_TYPE_DATA, BLUFI_DATA_SUBTYPE_CUSTOM_DATA, customData)
    return this.sendData(frame)
  }

  // 删除指定索引的WiFi配置
  deleteStoredConfig(index) {
    console.log('=== 请求删除WiFi配置，索引:', index, '===')
    // 发送自定义数据请求：类型 0x02 = 删除指定索引的WiFi配置
    const customData = [0x02, index]
    const frame = this.buildFrame(BLUFI_TYPE_DATA, BLUFI_DATA_SUBTYPE_CUSTOM_DATA, customData)
    return this.sendData(frame)
  }

  // 发送WiFi配置
  sendWifiConfig(ssid, password) {
    return new Promise(async (resolve, reject) => {
      try {
        // 发送 SSID
        const ssidBytes = this.stringToBytes(ssid)
        const ssidFrame = this.buildFrame(BLUFI_TYPE_DATA, BLUFI_DATA_SUBTYPE_STA_SSID, ssidBytes)
        await this.sendData(ssidFrame)
        await this.delay(100)
        
        // 发送密码
        if (password) {
          const passwdBytes = this.stringToBytes(password)
          const passwdFrame = this.buildFrame(BLUFI_TYPE_DATA, BLUFI_DATA_SUBTYPE_STA_PASSWD, passwdBytes)
          await this.sendData(passwdFrame)
          await this.delay(100)
        }
        
        // 发送连接命令
        const connectFrame = this.buildFrame(BLUFI_TYPE_CTRL, BLUFI_CTRL_SUBTYPE_CONNECT_WIFI)
        await this.sendData(connectFrame)
        
        resolve()
      } catch (err) {
        reject(err)
      }
    })
  }

  // 字符串转字节数组
  stringToBytes(str) {
    const bytes = []
    for (let i = 0; i < str.length; i++) {
      bytes.push(str.charCodeAt(i))
    }
    return bytes
  }

  // 字节数组转字符串
  bytesToString(bytes) {
    let str = ''
    for (let i = 0; i < bytes.length; i++) {
      str += String.fromCharCode(bytes[i])
    }
    return str
  }

  // 延迟函数
  delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms))
  }

  // 注册回调
  on(event, callback) {
    this.callbacks[event] = callback
  }

  // 断开连接
  disconnect() {
    return new Promise((resolve, reject) => {
      if (this.deviceId) {
        wx.closeBLEConnection({
          deviceId: this.deviceId,
          success: () => {
            this.deviceId = null
            // 重置序列号和状态
            this.sequence = 0
            this.receiveBuffer = []
            // 清空分包缓冲区
            this.fragmentBuffer = null
            this.fragmentExpectedLength = 0
            console.log('✓ 连接已断开，状态已重置')
            resolve()
          },
          fail: reject
        })
      } else {
        resolve()
      }
    })
  }
}

module.exports = BluFiProtocol
