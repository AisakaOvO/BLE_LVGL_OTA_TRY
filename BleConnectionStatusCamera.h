#ifndef ESP32_BLE_CONNECTION_STATUS_CAMERA_H
#define ESP32_BLE_CONNECTION_STATUS_CAMERA_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include <BLEServer.h>
#include "BLE2902.h"
#include "BLECharacteristic.h"

/**
 * 专用于 BleComboCamera 的连接状态管理类
 * 
 * 作用：
 * 1. 监听 BLE 服务器的连接/断开事件
 * 2. 管理相机控制特征的通知开关
 * 3. 维护连接状态标志
 */
class BleConnectionStatusCamera : public BLEServerCallbacks
{
public:
  BleConnectionStatusCamera(void);
  
  // 连接状态标志，公开访问，供外部查询
  bool connected = false;
  
  // BLE 服务器回调函数（由 ESP32 BLE 库自动调用）
  void onConnect(BLEServer* pServer) override;
  void onDisconnect(BLEServer* pServer) override;
  
  // 相机控制输入特征
  BLECharacteristic* inputCameraKeys;
};

#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_CONNECTION_STATUS_CAMERA_H