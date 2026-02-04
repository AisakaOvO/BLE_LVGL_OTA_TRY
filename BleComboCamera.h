#ifndef ESP32_BLE_COMBO_CAMERA_H
#define ESP32_BLE_COMBO_CAMERA_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "BleConnectionStatusCamera.h" //自定义连接通知状态管理类
#include "BLEHIDDevice.h"
#include "BLECharacteristic.h"

// 相机控制按键定义
typedef uint8_t CameraKeyReport[2];

// 音量控制
const CameraKeyReport KEY_CAMERA_VOLUME_UP = {1,0};      // bit 0
const CameraKeyReport KEY_CAMERA_VOLUME_DOWN = {2,0};     // bit 1

// 相机焦距控制
const CameraKeyReport KEY_CAMERA_ZOOM_IN = {4,0};        // bit 2 (放大)0000 0010
const CameraKeyReport KEY_CAMERA_ZOOM_OUT = {8,0};       // bit 3 (缩小)0000 0100

const CameraKeyReport KEY_CAMERA_SHOOT = {16,0};       // bit 4 (快门)0000 1000


class BleComboCamera
{
private:
  BleConnectionStatusCamera* connectionStatus;
  BLEHIDDevice* hid;
  BLECharacteristic* inputCameraKeys;
  
  CameraKeyReport _cameraKeyReport;
  static void taskServer(void* pvParameter);

public:
  BleComboCamera(std::string deviceName = "ESP32-Camera-Control", 
                 std::string deviceManufacturer = "Espressif", 
                 uint8_t batteryLevel = 100);
  void begin(void);
  void end(void);
  void sendReport(CameraKeyReport* keys);
  
  size_t press(const CameraKeyReport k);
  size_t release(const CameraKeyReport k);
  size_t write(const CameraKeyReport c);
  
  void releaseAll(void);
  bool isConnected(void);
  void setBatteryLevel(uint8_t level);
  
  uint8_t batteryLevel;
  std::string deviceManufacturer;
  std::string deviceName;
};

#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_COMBO_CAMERA_H