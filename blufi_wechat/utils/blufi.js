/**
 * BluFi 协议处理模块
 * 基于 ESP-IDF BluFi 协议实现
 */

const BluFiCrypto = require('./blufi_crypto.js')

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
    this.crypto = new BluFiCrypto()
    this.securityEnabled = false
    this.negotiationComplete = false
  }

  // 连接设备
  connect(deviceId) {
    return new Promise((resolve, reject) => {
      this.deviceId = deviceId
      
      wx.createBLEConnection({
        deviceId: deviceId,
        success: () => {
          console.log('BLE连接成功')
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
          resolve()
        },
        fail: reject
      })
    })
  }

  // 开始密钥协商
  startNegotiation() {
    return new Promise((resolve, reject) => {
      console.log('=== 开始密钥协商 ===')
      
      // 生成 DH 密钥对
      this.crypto.generatePrivateKey()
      const publicKey = this.crypto.generatePublicKey()
      
      console.log('客户端公钥已生成，长度:', publicKey.length)
      console.log('公钥前16字节:', publicKey.slice(0, 16))
      
      // 构建协商数据：类型(1字节) + 公钥(128字节)
      // SEC_TYPE_DH_PUBLIC = 0x04
      const negotiationData = [0x04, ...publicKey]
      
      // 发送公钥到设备
      const frame = this.buildFrame(BLUFI_TYPE_DATA, BLUFI_DATA_SUBTYPE_NEG, negotiationData)
      const frameData = new Uint8Array(frame)
      console.log('发送帧头:', Array.from(frameData.slice(0, 8)))
      console.log('帧总长度:', frameData.length)
      
      this.sendData(frame)
        .then(() => {
          console.log('公钥已发送，等待服务器响应...')
          // 等待服务器返回公钥
          this.negotiationCallback = resolve
          
          // 设置超时
          setTimeout(() => {
            if (!this.negotiationComplete) {
              reject(new Error('密钥协商超时'))
            }
          }, 5000)
        })
        .catch(reject)
    })
  }

  // 处理通知数据
  handleNotify(buffer) {
    const data = new Uint8Array(buffer)
    console.log('>>> 收到通知，长度:', data.length, '数据:', Array.from(data).slice(0, 20))
    
    // 解析 BluFi 帧
    const frame = this.parseFrame(data)
    if (frame) {
      this.handleFrame(frame)
    } else {
      console.warn('帧解析失败')
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
    
    if (data.length < 4 + dataLen) {
      console.warn('帧数据不完整')
      return null
    }
    
    let payload = data.slice(4, 4 + dataLen)
    
    console.log('Payload原始数据（前20字节）:', Array.from(payload.slice(0, 20)))
    
    // 检查是否需要解密
    if ((fc & BLUFI_FC_ENC) && this.securityEnabled) {
      console.log('解密数据...')
      payload = this.crypto.aesDecrypt(payload)
    } else if (fc & BLUFI_FC_ENC) {
      console.warn('⚠️ 数据有加密标志但加密未启用，数据可能无法正确解析')
    }
    
    // 检查校验和
    if (fc & BLUFI_FC_CHECK) {
      const checksumOffset = 4 + dataLen
      if (data.length >= checksumOffset + 2) {
        const receivedChecksum = data[checksumOffset] | (data[checksumOffset + 1] << 8)
        const calculatedChecksum = this.crypto.crc16(data.slice(0, checksumOffset))
        console.log('校验和:', {
          received: receivedChecksum,
          calculated: calculatedChecksum
        })
      }
    }
    
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
        case BLUFI_DATA_SUBTYPE_NEG:
          // 收到服务器公钥
          this.handleNegotiation(frame.payload)
          break
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
  handleNegotiation(payload) {
    console.log('收到协商数据，长度:', payload.length)
    
    if (payload.length < 2) {
      console.error('协商数据太短')
      return
    }
    
    const type = payload[0]
    console.log('协商数据类型:', type)
    
    // SEC_TYPE_DH_PUBLIC = 0x04
    if (type === 0x04) {
      const serverPublicKey = Array.from(payload.slice(1))
      console.log('收到服务器公钥，长度:', serverPublicKey.length)
      
      try {
        // 计算共享密钥
        this.crypto.computeSharedKey(serverPublicKey)
        this.negotiationComplete = true
        this.securityEnabled = true
        
        console.log('✓ 密钥协商完成，加密已启用')
        
        if (this.negotiationCallback) {
          this.negotiationCallback()
          this.negotiationCallback = null
        }
      } catch (err) {
        console.error('密钥协商失败:', err)
        if (this.negotiationCallback) {
          this.negotiationCallback(err)
        }
      }
    } else {
      console.warn('未知的协商数据类型:', type)
    }
  }

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
    let offset = 3
    while (offset < payload.length) {
      const type = payload[offset++]
      const len = payload[offset++]
      
      if (offset + len > payload.length) break
      
      const data = payload.slice(offset, offset + len)
      offset += len
      
      switch(type) {
        case 0x00: // SSID
          status.ssid = this.bytesToString(data)
          break
        case 0x01: // Password
          status.password = this.bytesToString(data)
          break
        case 0x02: // BSSID
          status.bssid = Array.from(data).map(b => b.toString(16).padStart(2, '0')).join(':')
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
    console.error('BluFi错误:', errorCode)
    
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
    
    // 如果安全已启用且不是协商帧，则加密
    if (this.securityEnabled && subtype !== BLUFI_DATA_SUBTYPE_NEG) {
      fc |= BLUFI_FC_ENC
      fc |= BLUFI_FC_CHECK
      
      // 加密 payload
      if (payload.length > 0) {
        actualPayload = Array.from(this.crypto.aesEncrypt(new Uint8Array(payload)))
      }
    }
    
    const frameLen = 4 + actualPayload.length + (fc & BLUFI_FC_CHECK ? 2 : 0)
    const frame = new Uint8Array(frameLen)
    
    frame[0] = (subtype << 2) | type
    frame[1] = fc
    frame[2] = this.sequence  // 先使用当前序列号
    frame[3] = actualPayload.length
    
    if (actualPayload.length > 0) {
      frame.set(actualPayload, 4)
    }
    
    // 添加校验和
    if (fc & BLUFI_FC_CHECK) {
      const checksum = this.crypto.crc16(frame.slice(0, 4 + actualPayload.length))
      frame[4 + actualPayload.length] = checksum & 0xFF
      frame[4 + actualPayload.length + 1] = (checksum >> 8) & 0xFF
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
            this.securityEnabled = false
            this.negotiationComplete = false
            this.receiveBuffer = []
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
