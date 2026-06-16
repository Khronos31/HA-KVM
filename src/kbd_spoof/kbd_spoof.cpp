// AtomS3U: キーボード ＋ システムコントロール(電源/スリープ/ウェイク) ＋ 音量 レシーバ。
// 普通のPCにUSBで挿して使う(切替器対応は断念)。
//  - キーボード : WebSocket 2バイト [hid, down]（既存receiverと同じ）
//  - 電源系     : HTTP  /power /sleep /wake （既存 /reboot と同じ流儀。HAから叩ける）
//  - 音量       : HTTP  /volup /voldown /mute
//  - 本体ボタン : 押す = スリープ（物理スリープボタン）
#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDSystemControl.h"
#include "USBHIDConsumerControl.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <WebServer.h>

using namespace websockets;

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_PASSWORD"
#endif
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const int BTN_PIN = 41; // AtomS3U 本体ボタン

USBHIDKeyboard Keyboard;
USBHIDSystemControl SystemControl;
USBHIDConsumerControl Consumer;

WebsocketsServer wsServer;
WebsocketsClient client;
WebServer httpServer(80);

unsigned long lastActivity = 0;
const unsigned long TIMEOUT_MS = 5000;

// システムコントロール(電源/スリープ/ウェイク)を1発送る
void sysTap(uint8_t v) {
    SystemControl.press(v);
    delay(60);
    SystemControl.release();
    lastActivity = millis();
}

// コンシューマ(音量等)を1発送る
void consumerTap(uint16_t v) {
    Consumer.press(v);
    delay(40);
    Consumer.release();
    lastActivity = millis();
}

void onBinaryMessage(WebsocketsClient& c, WebsocketsMessage message) {
    const uint8_t* data = (const uint8_t*)message.c_str();
    size_t len = message.length();

    if (len == 2) { // キーボード [hid, down]
        uint8_t code = data[0];
        bool isDown = (data[1] == 1);
        if (code >= 0x80 && code <= 0x87) {
            if (isDown) Keyboard.press(code); else Keyboard.release(code);
        } else {
            if (isDown) Keyboard.pressRaw(code); else Keyboard.releaseRaw(code);
        }
        lastActivity = millis();
    }
    else if (len == 3 && data[0] == 0xF0) { // システムコントロール [0xF0, action, down]
        bool isDown = (data[2] == 1);          // action: 1=Power 2=Sleep 3=Wake
        if (isDown) SystemControl.press(data[1]); else SystemControl.release();
        lastActivity = millis();
    }
}

void handleButton() {
    static int last = HIGH;
    int now = digitalRead(BTN_PIN);
    if (last == HIGH && now == LOW) { // 押下エッジ = スリープ
        sysTap(SYSTEM_CONTROL_STANDBY);
        delay(50);
    }
    last = now;
}

void setup() {
    // USBを先に上げる（WiFi未設定でもキーボード／ボタンとして動く）
    USB.VID(0x1A2C);
    USB.PID(0x0E24);
    USB.manufacturerName("SEM");
    USB.productName("USB Keyboard");
    Keyboard.begin();
    SystemControl.begin();
    Consumer.begin();
    USB.begin();

    pinMode(BTN_PIN, INPUT_PULLUP);

    WiFi.begin(ssid, password); // 非ブロッキング
    WiFi.setSleep(false);

    httpServer.on("/reboot", []() { httpServer.send(200, "text/plain", "reboot"); delay(300); ESP.restart(); });
    httpServer.on("/power",  []() { sysTap(SYSTEM_CONTROL_POWER_OFF);  httpServer.send(200, "text/plain", "power"); });
    httpServer.on("/sleep",  []() { sysTap(SYSTEM_CONTROL_STANDBY);    httpServer.send(200, "text/plain", "sleep"); });
    httpServer.on("/wake",   []() { sysTap(SYSTEM_CONTROL_WAKE_HOST);  httpServer.send(200, "text/plain", "wake"); });
    httpServer.on("/volup",  []() { consumerTap(CONSUMER_CONTROL_VOLUME_INCREMENT); httpServer.send(200, "text/plain", "volup"); });
    httpServer.on("/voldown",[]() { consumerTap(CONSUMER_CONTROL_VOLUME_DECREMENT); httpServer.send(200, "text/plain", "voldown"); });
    httpServer.on("/mute",   []() { consumerTap(CONSUMER_CONTROL_MUTE);             httpServer.send(200, "text/plain", "mute"); });
    httpServer.begin();
}

void loop() {
    handleButton();

    static bool wsStarted = false;
    if (!wsStarted && WiFi.status() == WL_CONNECTED) {
        wsServer.listen(81);
        wsStarted = true;
    }

    if (wsStarted) {
        httpServer.handleClient();
        if (wsServer.poll()) {
            WebsocketsClient newClient = wsServer.accept();
            if (newClient.available()) {
                if (client.available()) client.close();
                client = newClient;
                client.onMessage(onBinaryMessage);
            }
        }
        if (client.available()) client.poll();
    }

    if (millis() - lastActivity > TIMEOUT_MS) {
        Keyboard.releaseAll();
        lastActivity = millis();
    }

    delay(1);
}
