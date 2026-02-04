#include "BleConnectionStatusCamera.h"

/**
 * 构造函数
 */
BleConnectionStatusCamera::BleConnectionStatusCamera(void) {
  // 初始化为未连接状态
  connected = false;
  inputCameraKeys = nullptr;
}

/**
 * 当 BLE 客户端（手机/电脑）连接时自动调用
 * 
 * 调用时机：ESP32 BLE 协议栈检测到 GATT 连接建立
 * 调用者：ESP-IDF BLE 库内部（通过 BLEServer 分发）
 * 
 * 作用：
 * 1. 设置 connected 标志为 true
 * 2. 启用特征的通知功能（允许向客户端推送数据）
 */
void BleConnectionStatusCamera::onConnect(BLEServer* pServer)
{
  this->connected = true;
  
  // 安全检查：确保特征已初始化
  if (this->inputCameraKeys != nullptr) {
    // 获取 CCCD (Client Characteristic Configuration Descriptor)
    // 0x2902 是 BLE 标准定义的 CCCD UUID
    BLE2902* desc = (BLE2902*)this->inputCameraKeys->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    
    // 再次安全检查：确保描述符存在
    if (desc != nullptr) {
      // 启用通知：允许 ESP32 主动向客户端发送数据
      desc->setNotifications(true);
    }
  }
}

/**
 * 当 BLE 客户端断开连接时自动调用
 * 
 * 调用时机：客户端主动断开、超时、或连接丢失
 * 
 * 作用：
 * 1. 设置 connected 标志为 false
 * 2. 关闭通知功能（节省资源）
 */
void BleConnectionStatusCamera::onDisconnect(BLEServer* pServer)
{
  this->connected = false;
  
  // 安全检查
  if (this->inputCameraKeys != nullptr) {
    BLE2902* desc = (BLE2902*)this->inputCameraKeys->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    
    if (desc != nullptr) {
      // 关闭通知
      desc->setNotifications(false);
    }
  }
}

