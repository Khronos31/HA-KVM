#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include <WiFi.h>
#include <WebServer.h>
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_PASSWORD"
#endif
#ifndef DEVICE_NAME
#define DEVICE_NAME "kvm-unknown"
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

USBHIDKeyboard Keyboard;
USBHIDConsumerControl Consumer;
WebServer server(80);

unsigned long lastActivity = 0;
const unsigned long TIMEOUT_MS = 5000;

// HTTP GET: /key?code=65&state=2 (65='A', state: 1=Down, 0=Up, 2=Click)
void handleKey() {
    if (!server.hasArg("code") || !server.hasArg("state")) {
        server.send(400, "text/plain", "Missing 'code' or 'state' parameters");
        return;
    }

    uint8_t code = server.arg("code").toInt();
    int state = server.arg("state").toInt();

    // モディファイアキー (0x80 - 0x87) の判定
    if (code >= 0x80 && code <= 0x87) {
        if (state == 1) Keyboard.press(code);
        else if (state == 0) Keyboard.release(code);
        else { Keyboard.press(code); delay(50); Keyboard.release(code); }
    } else {
        if (state == 1) Keyboard.pressRaw(code);
        else if (state == 0) Keyboard.releaseRaw(code);
        else { Keyboard.pressRaw(code); delay(50); Keyboard.releaseRaw(code); }
    }
    
    lastActivity = millis();
    server.send(200, "text/plain", "Key processed OK");
}

// HTTP GET: /type?text=Hello (文字列を一気に入力する便利機能)
void handleType() {
    if (!server.hasArg("text")) {
        server.send(400, "text/plain", "Missing 'text' parameter");
        return;
    }
    
    String text = server.arg("text");
    Keyboard.print(text);
    
    lastActivity = millis();
    server.send(200, "text/plain", "Text typed OK");
}

// HTTP GET: /consumer?code=233&state=2 (233=VolumeUp)
void handleConsumer() {
    if (!server.hasArg("code")) return;

    uint16_t code = server.arg("code").toInt();
    int state = server.arg("state").toInt();

    if (state == 1) Consumer.press(code);
    else if (state == 0) Consumer.release();
    else { Consumer.press(code); delay(50); Consumer.release(); }
    
    server.send(200, "text/plain", "Consumer key OK");
}

void setup() {
    //Serial.begin(115200);
    
    Keyboard.begin();
    Consumer.begin();
    USB.begin();

    WiFi.setHostname(DEVICE_NAME);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    WiFi.setSleep(false);

    server.on("/key", HTTP_GET, handleKey);
    server.on("/type", HTTP_GET, handleType);
    server.on("/consumer", HTTP_GET, handleConsumer);
    server.on("/name", HTTP_GET, []() { server.send(200, "text/plain", DEVICE_NAME); });
    server.begin();
}

void loop() {
    server.handleClient();

    // 無操作時の安全対策 (キーの押しっぱなし解除)
    if (millis() - lastActivity > TIMEOUT_MS) {
        Keyboard.releaseAll();
        lastActivity = millis();
    }
    delay(1);
}