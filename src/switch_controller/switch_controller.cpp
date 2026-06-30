// AtomS3U を Nintendo Switch 用 USB HID ゲームパッドとして機能させるファームウェア。
// HORIPAD S (VID:0x0F0D / PID:0x00C1) に偽装するため、Switch が認証なしに認識する。
// WiFi 経由の HTTP API でボタン・十字キー・スティックを操作できる。
//
// API:
//   GET /press?btn=<name>&ms=<ms>  ボタンを押して離す (a/b/x/y/l/r/zl/zr/minus/plus/lstick/rstick/home/capture)
//   GET /hat?dir=<dir>&ms=<ms>     十字キーを押して離す (up/down/left/right/upright/downright/downleft/upleft)
//   GET /stick?lx=128&ly=128&rx=128&ry=128  アナログスティック位置を設定（0-255, 128=中央）
//   GET /release                   すべてのボタン・スティックを中立に戻す
//   GET /name                      デバイス名を返す
//   GET /reboot                    再起動

#include <Arduino.h>
#include "USB.h"
#include "USBHID.h"
#include <WiFi.h>
#include <WebServer.h>

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_PASSWORD"
#endif

// HORIPAD S 互換 HID ディスクリプタ (7バイトレポート)
// ボタン14bit + パディング2bit + ハット4bit + パディング4bit + 軸4x8bit
static const uint8_t SWITCH_HID_DESC[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Gamepad)
    0xA1, 0x01,        // Collection (Application)
    // --- ボタン 14bit ---
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x01,        //   Physical Maximum (1)
    0x75, 0x01,        //   Report Size (1 bit)
    0x95, 0x0E,        //   Report Count (14)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1 = Y)
    0x29, 0x0E,        //   Usage Maximum (Button 14 = Capture)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    // --- パディング 2bit ---
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x01,        //   Input (Constant)
    // --- ハット 4bit ---
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x25, 0x07,        //   Logical Maximum (7)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x75, 0x04,        //   Report Size (4 bits)
    0x95, 0x01,        //   Report Count (1)
    0x65, 0x14,        //   Unit (English Rotation)
    0x09, 0x39,        //   Usage (Hat Switch)
    0x81, 0x42,        //   Input (Data, Variable, Absolute, Null State)
    // --- パディング 4bit ---
    0x65, 0x00,        //   Unit (None)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Constant)
    // --- 軸 4x8bit (LX/LY/RX/RY) ---
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0xFF, 0x00,  //   Physical Maximum (255)
    0x75, 0x08,        //   Report Size (8 bits)
    0x95, 0x04,        //   Report Count (4)
    0x09, 0x30,        //   Usage (X)  = LX
    0x09, 0x31,        //   Usage (Y)  = LY
    0x09, 0x32,        //   Usage (Z)  = RX
    0x09, 0x35,        //   Usage (Rz) = RY
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0xC0,              // End Collection
};

USBHID HID;

class SwitchController : public USBHIDDevice {
public:
    struct __attribute__((packed)) Report {
        uint16_t buttons;   // ビット 0-13 = Y,B,A,X,L,R,ZL,ZR,-,+,LS,RS,Home,Capture
        uint8_t hat;        // 0=↑ 1=↗ 2=→ 3=↘ 4=↓ 5=↙ 6=← 7=↖ 8=中立
        uint8_t lx, ly;    // 左スティック (128=中央)
        uint8_t rx, ry;    // 右スティック (128=中央)
    };

    Report state = {0, 8, 128, 128, 128, 128};

    SwitchController() {
        HID.addDevice(this, sizeof(SWITCH_HID_DESC));
    }

    uint16_t _onGetDescriptor(uint8_t* dst) override {
        memcpy(dst, SWITCH_HID_DESC, sizeof(SWITCH_HID_DESC));
        return sizeof(SWITCH_HID_DESC);
    }

    bool send() {
        return HID.SendReport(0, &state, sizeof(state));
    }
} controller;

WebServer server(80);

// ボタン名 → ビット番号 (-1=不明)
static int btnBit(const String& name) {
    if (name == "y")       return 0;
    if (name == "b")       return 1;
    if (name == "a")       return 2;
    if (name == "x")       return 3;
    if (name == "l")       return 4;
    if (name == "r")       return 5;
    if (name == "zl")      return 6;
    if (name == "zr")      return 7;
    if (name == "minus")   return 8;
    if (name == "plus")    return 9;
    if (name == "lstick")  return 10;
    if (name == "rstick")  return 11;
    if (name == "home")    return 12;
    if (name == "capture") return 13;
    return -1;
}

// 方向名 → ハット値 (8=不明)
static uint8_t dirHat(const String& dir) {
    if (dir == "up")        return 0;
    if (dir == "upright")   return 1;
    if (dir == "right")     return 2;
    if (dir == "downright") return 3;
    if (dir == "down")      return 4;
    if (dir == "downleft")  return 5;
    if (dir == "left")      return 6;
    if (dir == "upleft")    return 7;
    return 8;
}

static int defaultMs(int ms) { return ms > 0 ? ms : 80; }

void setup() {
    USB.VID(0x0F0D);
    USB.PID(0x00C1);
    USB.manufacturerName("HORI CO.,LTD.");
    USB.productName("HORIPAD S");
    HID.begin();
    USB.begin();

    WiFi.setHostname("switch-controller");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(false);

    server.on("/press", []() {
        String btn = server.arg("btn");
        int ms = defaultMs(server.arg("ms").toInt());
        int bit = btnBit(btn);
        if (bit < 0) { server.send(400, "text/plain", "unknown button: " + btn); return; }
        controller.state.buttons |= (1u << bit);
        controller.send();
        delay(ms);
        controller.state.buttons &= ~(1u << bit);
        controller.send();
        server.send(200, "text/plain", "ok");
    });

    server.on("/hat", []() {
        String dir = server.arg("dir");
        int ms = defaultMs(server.arg("ms").toInt());
        uint8_t hat = dirHat(dir);
        if (hat == 8 && dir.length() > 0) { server.send(400, "text/plain", "unknown dir: " + dir); return; }
        controller.state.hat = hat;
        controller.send();
        delay(ms);
        controller.state.hat = 8;
        controller.send();
        server.send(200, "text/plain", "ok");
    });

    server.on("/stick", []() {
        if (server.hasArg("lx")) controller.state.lx = (uint8_t)constrain(server.arg("lx").toInt(), 0, 255);
        if (server.hasArg("ly")) controller.state.ly = (uint8_t)constrain(server.arg("ly").toInt(), 0, 255);
        if (server.hasArg("rx")) controller.state.rx = (uint8_t)constrain(server.arg("rx").toInt(), 0, 255);
        if (server.hasArg("ry")) controller.state.ry = (uint8_t)constrain(server.arg("ry").toInt(), 0, 255);
        controller.send();
        server.send(200, "text/plain", "ok");
    });

    server.on("/release", []() {
        controller.state = {0, 8, 128, 128, 128, 128};
        controller.send();
        server.send(200, "text/plain", "ok");
    });

    server.on("/name", []() {
        server.send(200, "text/plain", "switch-controller @ " + WiFi.localIP().toString());
    });

    server.on("/reboot", []() {
        server.send(200, "text/plain", "reboot");
        delay(300);
        ESP.restart();
    });

    server.begin();
}

void loop() {
    server.handleClient();
    delay(1);
}
