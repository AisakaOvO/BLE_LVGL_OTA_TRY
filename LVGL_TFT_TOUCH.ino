#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>
//#include <BleCombo.h>
#include <Touch_CST328.h>
#include <LVGL_Driver.h>
#include <BleComboCamera.h>
#include <OtaServer.h>


BleComboCamera Camera("Aisaka", "Espressif", 100);
//BleComboMouse mouse(&keyboard);

bool last_connected_status = false;
bool ota_mode_active = false;
// Create a label to show BLE status

WebServer server(80);

lv_obj_t *status_label = nullptr;
lv_obj_t *ota_btn_label = nullptr;
lv_obj_t *ota_btn = nullptr;

//实现持续zoom in/out
bool zoom_in_pressing = false ;
bool zoom_out_pressing = false ;
lv_timer_t *zoom_timer = nullptr;

static uint32_t ota_press_start_ms = 0;
static bool     ota_triggered      = false;

class MyCallbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* pCharacteristic) { }
  void onWrite(BLECharacteristic *pCharacteristic) { }
};


// Continuous zoom timer callback
static void continuous_zoom_timer(lv_timer_t *timer) {
    if (!Camera.isConnected()) {
        return;
    }
    
    if (zoom_in_pressing) {
        Camera.write(KEY_CAMERA_ZOOM_IN);
        Serial.println("Continuous Zoom In...");
    }
    
    if (zoom_out_pressing) {
        Camera.write(KEY_CAMERA_ZOOM_OUT);
        Serial.println("Continuous Zoom Out...");
    }
}

// Zoom In button callback
static void zoom_in_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        Serial.println("Zoom In button PRESSED");
        if (Camera.isConnected()) {
            zoom_in_pressing = true;
            Camera.write(KEY_CAMERA_ZOOM_IN);
            Serial.println("Zoom In started!");
            
            lv_obj_t *btn = lv_event_get_target(e);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA00), 0);
        }
    } 
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        Serial.println("Zoom In button RELEASED");
        zoom_in_pressing = false;
        
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2196F3), 0);
    }
}

// Zoom Out button callback
static void zoom_out_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        Serial.println("Zoom Out button PRESSED");
        if (Camera.isConnected()) {
            zoom_out_pressing = true;
            Camera.write(KEY_CAMERA_ZOOM_OUT);
            Serial.println("Zoom Out started!");
            
            lv_obj_t *btn = lv_event_get_target(e);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA00), 0);
        }
    } 
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        Serial.println("Zoom Out button RELEASED");
        zoom_out_pressing = false;
        
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2196F3), 0);
    }
}


// 按钮回调函数
static void volume_down_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

     Serial.println("LVGL click!!");

    if (code == LV_EVENT_CLICKED) {
        if (Camera.isConnected()) {
            Camera.write(KEY_CAMERA_VOLUME_DOWN);
            Serial.println("Volume Down sent!");
            
            //  show feedback
            lv_obj_t *label_down = (lv_obj_t *)lv_event_get_user_data(e);
            lv_label_set_text(label_down, "Volume down\nSent!");
            
        } else {
            Serial.println("BLE not connected!");
        }
    }
}
// 按钮回调函数(Volume up)
static void volume_up_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

     Serial.println("LVGL click!!");

    if (code == LV_EVENT_CLICKED) {
        if (Camera.isConnected()) {
            Camera.write(KEY_CAMERA_VOLUME_UP);
            Serial.println("Volume up sent!");
            
            // Optional: show feedback
            lv_obj_t *label_up = (lv_obj_t *)lv_event_get_user_data(e);
            lv_label_set_text(label_up, "Volume Down\nSent!");
            
        } else {
            Serial.println("BLE not connected!");
        }
    }
}

//
static void Zoom_in_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

     

    if (code == LV_EVENT_CLICKED) {
        if (Camera.isConnected()) {
            Camera.write(KEY_CAMERA_ZOOM_IN);
            zoom_in_pressing = true ;
            Serial.println("Volume up sent!");
            
            // Optional: show feedback
            lv_obj_t *label_zoom_in = (lv_obj_t *)lv_event_get_user_data(e);
            lv_label_set_text(label_zoom_in, "Volume Down\nSent!");
            
        } else {
            Serial.println("BLE not connected!");
        }
    }
}
//*******OTA服务按钮回调函数******//

//******************************//
static void ota_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        if (ota_mode_active) return;
        ota_press_start_ms = millis();
        ota_triggered      = false;
        lv_label_set_text(ota_btn_label, "OTA\n3s...");
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF9800), 0);  // 橙色：准备中

    } else if (code == LV_EVENT_PRESSING) {
        if (ota_mode_active || ota_triggered) return;

        uint32_t elapsed = millis() - ota_press_start_ms;

        if (elapsed >= 3000) {
            // ── 满 3 秒，触发 OTA 模式 ──
            ota_triggered = true;
            lv_label_set_text(ota_btn_label, "OTA\n连接中");
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xF44336), 0);  // 红色：连接中
            lv_timer_handler();  // 强制刷新一次屏幕，让"连接中"文字显示出来

            enter_ota_mode();    // 连接 WiFi + 启动 HTTP Server（阻塞最多约 10 秒）

            if (ota_mode_active) {
                // 连接成功：显示 IP 地址
                char ip_buf[24];
                snprintf(ip_buf, sizeof(ip_buf), "OTA\n%s", WiFi.localIP().toString().c_str());
                lv_label_set_text(ota_btn_label, ip_buf);
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x4CAF50), 0);  // 绿色：就绪
            } else {
                // 连接失败
                lv_label_set_text(ota_btn_label, "OTA\n失败");
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xF44336), 0);  // 红色：失败
                ota_triggered = false;
            }
        } else {
            // ── 显示倒计时 ──
            uint32_t remaining_secs = (3000 - elapsed + 999) / 1000;
            char buf[12];
            snprintf(buf, sizeof(buf), "OTA\n%lus...", remaining_secs);
            lv_label_set_text(ota_btn_label, buf);
        }

    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (ota_mode_active) return;  // OTA 已激活，按钮状态保持
        if (!ota_triggered) {
            // 松手时未到 3 秒，恢复原始状态
            lv_label_set_text(ota_btn_label, "OTA");
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x607D8B), 0);  // 灰蓝色：待机
        }
    }
}




// Create UI
void create_ui()
{

    status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label, "BLE: Waiting...");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, -20, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFF00), 0);
    
    //定时器注册
    lv_timer_create(update_ble_status_timer, 500, NULL);
    zoom_timer = lv_timer_create(continuous_zoom_timer, 100, NULL);

    // Create volume down button
    lv_obj_t *btn_down = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_down, 60, 40);
    lv_obj_align(btn_down, LV_ALIGN_BOTTOM_LEFT, 10, -20);

    // Create button label
    lv_obj_t *label_down = lv_label_create(btn_down);
    lv_label_set_text(label_down, "Volume--");
    lv_obj_center(label_down);


    // Volume Up button
    lv_obj_t *btn_up = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_up, 60, 40);
    lv_obj_align(btn_up, LV_ALIGN_BOTTOM_RIGHT, -10, -20);
    lv_obj_t *label_up = lv_label_create(btn_up);
    lv_label_set_text(label_up, "Volume++");
    lv_obj_center(label_up);



   // Zoom Out button
    lv_obj_t *btn_zoom_out = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_zoom_out, 80, 60);
    lv_obj_align(btn_zoom_out, LV_ALIGN_BOTTOM_LEFT, 20, -100);
    
    lv_obj_t *label_zoom_out = lv_label_create(btn_zoom_out);
    lv_label_set_text(label_zoom_out, "ZOOM\n----");
    lv_obj_center(label_zoom_out);
    lv_obj_add_event_cb(btn_zoom_out, zoom_out_btn_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn_zoom_out, zoom_out_btn_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn_zoom_out, zoom_out_btn_event_cb, LV_EVENT_PRESS_LOST, NULL);
    
    // Zoom In button
    lv_obj_t *btn_zoom_in = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_zoom_in, 80, 60);
    lv_obj_align(btn_zoom_in, LV_ALIGN_BOTTOM_RIGHT, -20, -100);
    
    lv_obj_t *label_zoom_in = lv_label_create(btn_zoom_in);
    lv_label_set_text(label_zoom_in, "ZOOM\n+");
    lv_obj_center(label_zoom_in);
    lv_obj_add_event_cb(btn_zoom_in, zoom_in_btn_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn_zoom_in, zoom_in_btn_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn_zoom_in, zoom_in_btn_event_cb, LV_EVENT_PRESS_LOST, NULL);


    // ── OTA 升级按钮（左上角，长按 3 秒进入 OTA 模式）──
    ota_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(ota_btn, 70, 50);
    lv_obj_align(ota_btn, LV_ALIGN_TOP_LEFT, 5, 25);
    lv_obj_set_style_bg_color(ota_btn, lv_color_hex(0x607D8B), 0);  // 初始灰蓝色

    ota_btn_label = lv_label_create(ota_btn);
    lv_label_set_text(ota_btn_label, "OTA");
    lv_obj_set_style_text_font(ota_btn_label, &lv_font_montserrat_12, 0);
    lv_obj_center(ota_btn_label);

    lv_obj_add_event_cb(ota_btn, ota_btn_event_cb, LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(ota_btn, ota_btn_event_cb, LV_EVENT_PRESSING,   NULL);
    lv_obj_add_event_cb(ota_btn, ota_btn_event_cb, LV_EVENT_RELEASED,   NULL);
    lv_obj_add_event_cb(ota_btn, ota_btn_event_cb, LV_EVENT_PRESS_LOST, NULL);


    // Add event callback
    lv_obj_add_event_cb(btn_down, volume_down_btn_event_cb, LV_EVENT_CLICKED, label_down);
    lv_obj_add_event_cb(btn_up, volume_up_btn_event_cb, LV_EVENT_CLICKED, label_up);
    // Optional: Create additional buttons for other functions
    

}


//状态监控定时器
static void update_ble_status_timer(lv_timer_t *timer){
    bool current_state = Camera.isConnected();
    
    if(current_state !=last_connected_status){

            if (current_state) {
            lv_label_set_text(status_label, "BLE: Connected");
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
            } else {
            lv_label_set_text(status_label, "BLE: Disconnected");
            lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
        }
        last_connected_status = current_state;

    }

}



void setup() {
    Serial.begin(115200);
    Serial.println("Starting work!");
    

    // Initialize touch
    Touch_Init();
    
    // Backlight on
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, 1);


    Lvgl_Init();
 
    // Create UI elements
    create_ui();

    // Initialize BLE keyboard
    MyCallbacks myCallbacks;
   //Camera.setCallbacks(&myCallbacks);
    Camera.begin();
   // mouse.begin();
    
    Serial.println("Setup complete!");
}

void loop() {
    if(ota_mode_active){

        server.handleClient();

    }


    lv_timer_handler();
    delay(5);
}