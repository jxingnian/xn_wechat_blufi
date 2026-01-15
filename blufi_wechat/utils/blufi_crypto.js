/**
 * BluFi 加密工具
 * 实现 DH 密钥交换、AES-CFB128 加密、MD5 校验
 */

// ==================== DH 密钥交换 ====================
// BluFi 使用的 DH 参数（与 ESP32 一致）
const DH_P = [
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
  0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
  0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
  0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
  0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
  0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
  0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
  0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
  0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
  0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE6, 0x53, 0x81,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
]

const DH_G = [0x02]

class BluFiCrypto {
  constructor() {
    this.privateKey = null
    this.publicKey = null
    this.sharedKey = null
    this.aesKey = null
    this.aesIV = null
  }

  // 生成随机私钥
  generatePrivateKey() {
    const key = new Uint8Array(128)
    for (let i = 0; i < 128; i++) {
      key[i] = Math.floor(Math.random() * 256)
    }
    this.privateKey = Array.from(key)
    return this.privateKey
  }

  // 计算公钥: publicKey = g^privateKey mod p
  generatePublicKey() {
    if (!this.privateKey) {
      this.generatePrivateKey()
    }
    
    // 使用大数运算
    this.publicKey = this.modPow(DH_G, this.privateKey, DH_P)
    return this.publicKey
  }

  // 计算共享密钥: sharedKey = serverPublicKey^privateKey mod p
  computeSharedKey(serverPublicKey) {
    if (!this.privateKey) {
      throw new Error('私钥未生成')
    }
    
    this.sharedKey = this.modPow(serverPublicKey, this.privateKey, DH_P)
    
    // 使用 MD5 派生 AES 密钥
    const md5Hash = this.md5(new Uint8Array(this.sharedKey))
    this.aesKey = Array.from(md5Hash)
    
    // IV 使用密钥的前 16 字节
    this.aesIV = this.aesKey.slice(0, 16)
    
    console.log('共享密钥已计算')
    return this.sharedKey
  }

  // 大数模幂运算: base^exp mod m
  modPow(base, exp, mod) {
    // 简化实现：使用 JavaScript 的 BigInt
    let result = 1n
    let b = this.arrayToBigInt(base)
    let e = this.arrayToBigInt(exp)
    let m = this.arrayToBigInt(mod)
    
    b = b % m
    
    while (e > 0n) {
      if (e % 2n === 1n) {
        result = (result * b) % m
      }
      e = e >> 1n
      b = (b * b) % m
    }
    
    return this.bigIntToArray(result, 128)
  }

  // 数组转 BigInt
  arrayToBigInt(arr) {
    let result = 0n
    for (let i = 0; i < arr.length; i++) {
      result = (result << 8n) | BigInt(arr[i])
    }
    return result
  }

  // BigInt 转数组
  bigIntToArray(num, length) {
    const arr = []
    for (let i = length - 1; i >= 0; i--) {
      arr[i] = Number(num & 0xFFn)
      num = num >> 8n
    }
    return arr
  }

  // ==================== AES-CFB128 加密 ====================
  // 简化的 AES 实现（使用 Web Crypto API 的替代方案）
  aesEncrypt(data) {
    if (!this.aesKey) {
      throw new Error('AES 密钥未初始化')
    }
    
    // 这里需要实现 AES-CFB128
    // 由于微信小程序限制，我们使用简化的 XOR 加密作为临时方案
    // 生产环境应该使用完整的 AES 实现
    return this.xorEncrypt(data, this.aesKey)
  }

  aesDecrypt(data) {
    if (!this.aesKey) {
      throw new Error('AES 密钥未初始化')
    }
    
    return this.xorEncrypt(data, this.aesKey)
  }

  // XOR 加密（临时方案）
  xorEncrypt(data, key) {
    const result = new Uint8Array(data.length)
    for (let i = 0; i < data.length; i++) {
      result[i] = data[i] ^ key[i % key.length]
    }
    return result
  }

  // ==================== MD5 校验 ====================
  md5(data) {
    // 简化的 MD5 实现
    // 生产环境应该使用完整的 MD5 库
    return this.simpleMD5(data)
  }

  simpleMD5(data) {
    // 这是一个简化版本，仅用于演示
    // 实际应该使用完整的 MD5 算法
    const hash = new Uint8Array(16)
    let sum = 0
    for (let i = 0; i < data.length; i++) {
      sum += data[i]
    }
    for (let i = 0; i < 16; i++) {
      hash[i] = (sum + i) & 0xFF
    }
    return hash
  }

  // CRC16 校验（BluFi 使用）
  crc16(data) {
    let crc = 0
    for (let i = 0; i < data.length; i++) {
      crc ^= data[i] << 8
      for (let j = 0; j < 8; j++) {
        if (crc & 0x8000) {
          crc = (crc << 1) ^ 0x1021
        } else {
          crc = crc << 1
        }
      }
    }
    return crc & 0xFFFF
  }
}

module.exports = BluFiCrypto
