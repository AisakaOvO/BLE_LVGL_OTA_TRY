#include "HardwareSerial.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>
#include <FS.h>
using namespace fs;
#include <WebServer.h>



#define WIFI_SSID "aisaka2"
#define WIFI_PASSWOED "1154855351"



extern WebServer server;
extern bool last_connected_status;
extern bool ota_mode_active;

//-----------------------
//GET / api / info
//返回设备info 
//字段根据上位机中 DeviceInfo.cs要求的一样

//-----------------------

void handle_api_info(){
    StaticJsonDocument<512> doc ;

    uint8_t mac[6];         // storage mac address
    WiFi.macAddress(mac);   
    char mac_str[18];
    snprintf(mac_str,sizeof(mac_str),

            "%02X:%02X:%02X:%02X:%02X:%02X",

            mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);


    char device_id[16];     // storage device id 
    snprintf(device_id,sizeof(device_id),

            "ESP32-%02X%02X",

            mac[4], mac[5]
            );
    //构筑device info
    doc["deviceId"]   = device_id;
    doc["deviceName"] = "BLE Camera Controller";
    doc["firmwareVersion"]= "1.0.0";         
    doc["buildDate"]      = __DATE__;
    doc["buildTime"]      = __TIME__;
    doc["ipAddress"]      = WiFi.localIP().toString();
    doc["macAddress"]     = mac_str;
    doc["rssi"]           = WiFi.RSSI();
    doc["freeHeap"]       = (int)ESP.getFreeHeap();
    doc["chipModel"]      = "ESP32";
    doc["chipRevision"]   = (int)ESP.getChipRevision();
    doc["flashSize"]      = (long)ESP.getFlashChipSize();
    doc["bleConnected"]   = last_connected_status;           // 
    doc["uptime"]         = (long)(millis() / 1000);

    String response;
    serializeJson(doc, response);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);

}

//─────────────────────────────────────────────
// POST /api/ota/prepare
// 检查 OTA 可用性，通知设备准备接收固件
// ─────────────────────────────────────────────

// ─────────────────────────────────────────────
// POST /api/ota/prepare
// 检查 OTA 可用性，通知设备准备接收固件
// ─────────────────────────────────────────────
void handle_ota_prepare() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Ready for OTA\"}");
}

// ─────────────────────────────────────────────
// POST /api/ota/upload
// 接收 multipart/form-data 格式的 .bin 固件并写入 Flash
// 上位机发送字段名为 "update"，见 FirmwareUploader.cs L57
// ─────────────────────────────────────────────
void handle_ota_upload() {
    server.sendHeader("Access-Control-Allow-Origin", "*");

    if (Update.hasError()) {
        server.send(200, "application/json",
            "{\"success\":false,\"error\":\"Update failed\"}");
    } else {
        server.send(200, "application/json", "{\"success\":true}");
        delay(500);
        ESP.restart();
    }
}

// 上传过程回调（每次收到数据块时调用）
void handle_ota_upload_file() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] 开始接收: %s\n", upload.filename.c_str());

        // 验证文件名或魔数（.bin 文件首字节为 0xE9）
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // 写入数据块到 Flash
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        Serial.printf("[OTA] 已接收: %u bytes\n", upload.totalSize);

    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[OTA] 升级完成，总大小: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

// ─────────────────────────────────────────────
// POST /api/reboot
// ─────────────────────────────────────────────
void handle_reboot() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"success\":true}");
    delay(500);
    ESP.restart();
}

// ─────────────────────────────────────────────
// 注册所有路由（在 setup() 中调用此函数）
// ─────────────────────────────────────────────
void ota_server_setup() {
    server.on("/api/info",        HTTP_GET,  handle_api_info);
    server.on("/api/ota/prepare", HTTP_POST, handle_ota_prepare);
    server.on("/api/reboot",      HTTP_POST, handle_reboot);

    // upload 路由需要同时绑定响应回调和文件回调
    server.on("/api/ota/upload", HTTP_POST,
        handle_ota_upload,       // 上传完成后的响应
        handle_ota_upload_file   // 每个数据块到达时的处理
    );

    server.begin();
    Serial.println("[OTA] HTTP Server 已启动");
}

void enter_ota_mode(){
    Serial.println("进入OTA模式");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID,WIFI_PASSWOED);

    Serial.println("[wifi]正在连接");
    int retry = 0 ;
    while(WiFi.status() != WL_CONNECTED && retry < 20){

        delay(500);
        Serial.print(".");
        retry++;

    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] 已连接，IP: %s\n", WiFi.localIP().toString().c_str());
        ota_server_setup();
        ota_mode_active = true;
    } else {
        Serial.println("\n[WiFi] 连接失败，请检查 SSID/密码");
        ota_mode_active = false;
    }



}
