#include <Arduino.h>
#include "M5AtomS3.h"
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <WebServer.h>

using namespace websockets;
WebsocketsServer wsServer;
WebsocketsClient client;  // グローバルに保持してブロッキングを回避
WebServer httpServer(80); // リブート用HTTPサーバー
USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

unsigned long lastActivity = 0;
const unsigned long TIMEOUT_MS = 5000;

void handleMouseInput16(uint8_t buttons, int16_t x, int16_t y, int8_t wheel) {
    if (x != 0 || y != 0 || wheel != 0) {
        Mouse.move(x, y, wheel);
    }

    if (buttons & 0x01) Mouse.press(MOUSE_LEFT);     else Mouse.release(MOUSE_LEFT);
    if (buttons & 0x02) Mouse.press(MOUSE_RIGHT);    else Mouse.release(MOUSE_RIGHT);
    if (buttons & 0x04) Mouse.press(MOUSE_MIDDLE);   else Mouse.release(MOUSE_MIDDLE);
    if (buttons & 0x08) Mouse.press(MOUSE_BACKWARD); else Mouse.release(MOUSE_BACKWARD);
    if (buttons & 0x10) Mouse.press(MOUSE_FORWARD);  else Mouse.release(MOUSE_FORWARD);
    
    lastActivity = millis();
}

void onBinaryMessage(WebsocketsClient& client, WebsocketsMessage message) {
    const uint8_t* data = (const uint8_t*)message.c_str();
    size_t len = message.length();

    if (len == 2) { // キーボード入力
        uint8_t code = data[0];
        bool isDown = (data[1] == 1);
        if (code >= 0x80 && code <= 0x87) {
            if (isDown) Keyboard.press(code); else Keyboard.release(code);
        } else {
            if (isDown) Keyboard.pressRaw(code); else Keyboard.releaseRaw(code);
        }
        lastActivity = millis();
    } 
    else if (len == 6) { // 6バイト: マウスレポート
        // Little Endianで復元 (16-bit)
        int16_t x = (int16_t)(data[1] | (data[2] << 8));
        int16_t y = (int16_t)(data[3] | (data[4] << 8));
        handleMouseInput16(data[0], x, y, (int8_t)data[5]);
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Keyboard.begin();
    Mouse.begin();
    USB.begin();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    WiFi.setSleep(false);

    wsServer.listen(81);

    // リブート用エンドポイントの設定
    httpServer.on("/reboot", []() {
        httpServer.send(200, "text/plain", "Rebooting M5AtomS3...");
        delay(500);
        ESP.restart();
    });
    httpServer.begin();
}

void loop() {
    httpServer.handleClient(); // Webサーバーの処理を回す

    // 新しいクライアントからの接続を受け付ける
    if (wsServer.poll()) {
        WebsocketsClient newClient = wsServer.accept();
        if (newClient.available()) {
            // 既存の接続があれば破棄する
            if (client.available()) {
               client.close();
            }
            client = newClient; // 新しい接続で上書き
            client.onMessage(onBinaryMessage);
        }
    }
        
    // 接続中のクライアントがある場合のみ処理
    if (client.available()) {
        client.poll();
    }
        
    // 無操作時の安全対策 (接続状態に関わらず常駐させることで切断直後の押しっぱなしも防ぐ)
    if (millis() - lastActivity > TIMEOUT_MS) {
        Keyboard.releaseAll();
        Mouse.release(MOUSE_ALL);
        lastActivity = millis();
    }

    delay(1);
}
