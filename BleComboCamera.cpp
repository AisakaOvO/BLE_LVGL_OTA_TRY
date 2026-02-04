#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "sdkconfig.h"

#include "BleConnectionStatusCamera.h"
#include "BleComboCamera.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
  #include "esp32-hal-log.h"
  #define LOG_TAG ""
#else
  #include "esp_log.h"
  static const char* LOG_TAG = "BLEDevice";
#endif

// Report ID
#define CAMERA_KEYS_ID 0x01

// 相机控制专用 HID 报告描述符
static const uint8_t _hidReportDescriptor[] = {
  // ------------------------------------------------- Camera Control Keys
  0x05, 0x0C,                    // USAGE_PAGE (Consumer)
  0x09, 0x01,                    // USAGE (Consumer Control)
  0xA1, 0x01,                    // COLLECTION (Application)
  0x85, CAMERA_KEYS_ID,          //   REPORT_ID (1)
  0x05, 0x0C,                    //   USAGE_PAGE (Consumer)
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,                    //   REPORT_SIZE (1)
  0x95, 0x08,                    //   REPORT_COUNT (8) - 8 bits
  
  // Bit 0: 音量加
  0x09, 0xE9,                    //   USAGE (Volume Increment)
  
  // Bit 1: 音量减
  0x09, 0xEA,                    //   USAGE (Volume Decrement)
  
  // Bit 2: 焦距放大
  0x0A, 0x2D, 0x02,              //   USAGE (Zoom In) - 0x022D
  
  // Bit 3: 焦距缩小
  0x0A, 0x2E, 0x02,              //   USAGE (Zoom Out) - 0x022E
  
  // Bit 4: 快门 - 可选
  0x0A, 0x21, 0x09,              //   USAGE (Camera Shutter) - 0x0921
  
  // Bit 5-7: 预留
  0x09, 0x00,                    //   USAGE (Unassigned)
  0x09, 0x00,                    //   USAGE (Unassigned)
  0x09, 0x00,                    //   USAGE (Unassigned)
  
  0x81, 0x02,                    //   INPUT (Data,Var,Abs)
  
  // 第二个字节（8位）全部预留
  0x95, 0x08,                    //   REPORT_COUNT (8)
  0x81, 0x01,                    //   INPUT (Const,Array,Abs) - padding
  
  0xC0                           // END_COLLECTION
};

BleComboCamera::BleComboCamera(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) : hid(0)
{
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->connectionStatus = new BleConnectionStatusCamera();
}

void BleComboCamera::begin(void)
{
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

void BleComboCamera::end(void)
{
}

bool BleComboCamera::isConnected(void) {
  return this->connectionStatus->connected;
}

void BleComboCamera::setBatteryLevel(uint8_t level) {
  this->batteryLevel = level;
  if (hid != 0)
    this->hid->setBatteryLevel(this->batteryLevel);
}

void BleComboCamera::taskServer(void* pvParameter) {

  BleComboCamera* bleCameraInstance = (BleComboCamera *) pvParameter;
  
  //初始化BLE设备
  BLEDevice::init(bleCameraInstance->deviceName.c_str());

  //建立服务器
  BLEServer *pServer = BLEDevice::createServer();
  // 3. 注册连接状态回调
  //   当客户端连接/断开时，ESP32 BLE 库会自动调用 connectionStatus 的方法
  pServer->setCallbacks(bleCameraInstance->connectionStatus);
  // 4. 创建 HID 设备
  bleCameraInstance->hid = new BLEHIDDevice(pServer);
  //5.设置report ID
  bleCameraInstance->inputCameraKeys = bleCameraInstance->hid->inputReport(CAMERA_KEYS_ID);
  //6.关键步骤：将特征指针传递给连接状态管理器
  //    这样 onConnect/onDisconnect 就能访问这个特征了
  bleCameraInstance->connectionStatus-> inputCameraKeys = bleCameraInstance->inputCameraKeys;
  //7.设备信息
  bleCameraInstance->hid->manufacturer()->setValue(bleCameraInstance->deviceManufacturer.c_str());
  bleCameraInstance->hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  bleCameraInstance->hid->hidInfo(0x00, 0x01);
 //8.安全性
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
 //9.加载HID 报告符
  bleCameraInstance->hid->reportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  bleCameraInstance->hid->startServices();
 //配置BLE 广播
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);  // 使用键盘外观（相机控制器没有专用外观码）
  pAdvertising->addServiceUUID(bleCameraInstance->hid->hidService()->getUUID());
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->start();

  //设置电池电量
  bleCameraInstance->hid->setBatteryLevel(bleCameraInstance->batteryLevel);

  ESP_LOGD(LOG_TAG, "Camera Control Advertising started!");
  vTaskDelay(portMAX_DELAY); 
}

void BleComboCamera::sendReport(CameraKeyReport* keys)
{
  if (this->isConnected())
  {
    this->inputCameraKeys->setValue((uint8_t*)keys, sizeof(CameraKeyReport));
    this->inputCameraKeys->notify();
  }
}

size_t BleComboCamera::press(const CameraKeyReport k)
{
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t cameraKeyReport_16 = _cameraKeyReport[1] | (_cameraKeyReport[0] << 8);

    cameraKeyReport_16 |= k_16;
    _cameraKeyReport[0] = (uint8_t)((cameraKeyReport_16 & 0xFF00) >> 8);
    _cameraKeyReport[1] = (uint8_t)(cameraKeyReport_16 & 0x00FF);

    sendReport(&_cameraKeyReport);
    return 1;
}

size_t BleComboCamera::release(const CameraKeyReport k)
{
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t cameraKeyReport_16 = _cameraKeyReport[1] | (_cameraKeyReport[0] << 8);
    
    cameraKeyReport_16 &= ~k_16;
    _cameraKeyReport[0] = (uint8_t)((cameraKeyReport_16 & 0xFF00) >> 8);
    _cameraKeyReport[1] = (uint8_t)(cameraKeyReport_16 & 0x00FF);

    sendReport(&_cameraKeyReport);
    return 1;
}

size_t BleComboCamera::write(const CameraKeyReport c)
{
    uint16_t p = press(c);
    release(c);
    return p;
}

void BleComboCamera::releaseAll(void)
{
    _cameraKeyReport[0] = 0;
    _cameraKeyReport[1] = 0;
    sendReport(&_cameraKeyReport);
}