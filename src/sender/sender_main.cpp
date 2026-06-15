#include <Arduino.h>
#include "USB.h"
#include "USBHID.h"
#define USE_NIMBLE
#include <NimBLEDevice.h>

// ------------------------------------------
// DualSense Report Descriptor
// ------------------------------------------
static const uint8_t dualsense_report_desc[] = {
  0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35,
  0x09, 0x33, 0x09, 0x34, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x06, 0x81, 0x02, 0x06,
  0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02, 0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
  0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x65, 0x00, 0x05,
  0x09, 0x19, 0x01, 0x29, 0x0F, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0F, 0x81, 0x02, 0x06,
  0x00, 0xFF, 0x09, 0x21, 0x95, 0x0D, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x09, 0x22, 0x15, 0x00, 0x26,
  0xFF, 0x00, 0x75, 0x08, 0x95, 0x34, 0x81, 0x02, 0x85, 0x02, 0x09, 0x23, 0x95, 0x2F, 0x91, 0x02,
  0x85, 0x05, 0x09, 0x33, 0x95, 0x28, 0xB1, 0x02, 0x85, 0x08, 0x09, 0x34, 0x95, 0x2F, 0xB1, 0x02,
  0x85, 0x09, 0x09, 0x24, 0x95, 0x13, 0xB1, 0x02, 0x85, 0x0A, 0x09, 0x25, 0x95, 0x1A, 0xB1, 0x02,
  0x85, 0x20, 0x09, 0x26, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x21, 0x09, 0x27, 0x95, 0x04, 0xB1, 0x02,
  0x85, 0x22, 0x09, 0x40, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x80, 0x09, 0x28, 0x95, 0x3F, 0xB1, 0x02,
  0x85, 0x81, 0x09, 0x29, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x82, 0x09, 0x2A, 0x95, 0x09, 0xB1, 0x02,
  0x85, 0x83, 0x09, 0x2B, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0x84, 0x09, 0x2C, 0x95, 0x3F, 0xB1, 0x02,
  0x85, 0x85, 0x09, 0x2D, 0x95, 0x02, 0xB1, 0x02, 0x85, 0xA0, 0x09, 0x2E, 0x95, 0x01, 0xB1, 0x02,
  0x85, 0xE0, 0x09, 0x2F, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0xF0, 0x09, 0x30, 0x95, 0x3F, 0xB1, 0x02,
  0x85, 0xF1, 0x09, 0x31, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0xF2, 0x09, 0x32, 0x95, 0x0F, 0xB1, 0x02,
  0x85, 0xF4, 0x09, 0x35, 0x95, 0x3F, 0xB1, 0x02, 0x85, 0xF5, 0x09, 0x36, 0x95, 0x03, 0xB1, 0x02,
  0xC0
};

USBHID hid;

class FakeDualSense : public USBHIDDevice {
public:
    void begin() {
        hid.addDevice(this, sizeof(dualsense_report_desc));
    }
    
    uint16_t _onGetDescriptor(uint8_t* buffer) {
        memcpy(buffer, dualsense_report_desc, sizeof(dualsense_report_desc));
        return sizeof(dualsense_report_desc);
    }
    
    void sendControllerData(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry, uint8_t seq, uint16_t buttons) {
        uint8_t report[63] = {0};
        report[0] = lx;   // X
        report[1] = ly;   // Y
        report[2] = rx;   // Z
        report[3] = ry;   // Rz
        report[4] = seq;  // Rx をパケットのシーケンス番号（更新検知）として利用
        report[5] = 0x00; // Ry 
        
        // Buttons
        report[7] = 0x08 | (buttons << 4);
        report[8] = (buttons >> 4);

        hid.SendReport(0x01, report, sizeof(report));
    }

    // 本番用: seqをスティック(report[0]=変化検知)、dx/dyをクリーンなトリガー(report[4],[5])に載せる
    void sendMouse(uint8_t seq, uint8_t txByte, uint8_t tyByte, uint16_t buttons) {
        uint8_t report[63] = {0};
        report[0] = seq;     // X  -> axes[0]   変化検知用seq（値の正確さは不要）
        report[1] = 0x80;    // Y  中立
        report[2] = 0x80;    // Z  中立
        report[3] = 0x80;    // Rz 中立
        report[4] = txByte;  // Rx -> buttons[6] (dx + 128)
        report[5] = tyByte;  // Ry -> buttons[7] (dy + 128)
        report[7] = 0x08 | (buttons << 4);
        report[8] = (buttons >> 4);
        hid.SendReport(0x01, report, sizeof(report));
    }

    // 計測用: 既知のテスト値 t を stick(report[0]) と trigger(report[4],[5]) に同時に載せる
    void sendCalib(uint8_t t) {
        uint8_t report[63] = {0};
        report[0] = t;    // X  -> axes[0]   スティック(iOSの補正あり)
        report[1] = 0x80; // Y  中立
        report[2] = 0x80; // Z
        report[3] = 0x80; // Rz
        report[4] = t;    // Rx -> buttons[6] L2トリガー(補正なし想定)
        report[5] = t;    // Ry -> buttons[7] R2トリガー
        report[7] = 0x08; // buttons 中立
        hid.SendReport(0x01, report, sizeof(report));
    }
};

FakeDualSense dualsense;
const char* TARGET_MOUSE_ADDR = "a8:10:87:18:f6:37";
static bool mouseConnected = false;
static NimBLEClient* pClient = nullptr;
uint8_t packet_seq = 0; // 送信パケットのシーケンス番号（変化検知用、スティックに載せる）

// マウス報告(高レート)を取りこぼさないため累積し、loop側で約60Hzでまとめて送る
portMUX_TYPE accMux = portMUX_INITIALIZER_UNLOCKED;
volatile int32_t accX = 0;
volatile int32_t accY = 0;
volatile int16_t accWheel = 0;
volatile uint16_t latchBtns = 0;
const uint32_t EMIT_MS = 16; // 約60Hz

void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 6) {
        // マウスは16bitのdx/dy(下位,上位の順)を報告する
        int16_t dx = (int16_t)(pData[1] | (pData[2] << 8));
        int16_t dy = (int16_t)(pData[3] | (pData[4] << 8));
        int8_t wheel = (int8_t)pData[5];
        uint16_t btns = pData[0] & 0x1F;

        // ここでは送らず累積するだけ（取りこぼし防止）
        portENTER_CRITICAL(&accMux);
        accX += dx;
        accY += dy;
        accWheel += wheel;
        latchBtns = btns;
        portEXIT_CRITICAL(&accMux);
    }
}

bool subscribeHID(NimBLEClient* pClient) {
    NimBLERemoteService* pSvc = pClient->getService("1812");
    if (pSvc) {
        auto charList = pSvc->getCharacteristics(true);
        for (auto &pChr : *charList) {
            if (pChr->getUUID() == NimBLEUUID("2A4D") && pChr->canNotify()) {
                if (pChr->subscribe(true, notifyCallback)) {
                    pClient->updateConnParams(6, 12, 0, 60);
                    return true;
                }
            }
        }
    }
    return false;
}

class MyClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) { mouseConnected = true; }
    void onDisconnect(NimBLEClient* pClient) { mouseConnected = false; }
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) {
        if (pClient) subscribeHID(pClient);
    }
};

void setup() {
    USB.VID(0x054C);
    USB.PID(0x0CE6);
    USB.manufacturerName("Sony Interactive Entertainment");
    USB.productName("Wireless Controller");

    dualsense.begin();
    hid.begin();
    USB.begin();

#ifndef CALIB_MODE
    NimBLEDevice::init("");
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
#endif
}

// 累積分を±127にクランプしてトリガーに載せ、超過分は次フレームへ繰り越す（シグマ・デルタ）
void emitMouse() {
    int32_t sx, sy; int16_t sw; uint16_t rawBtns;
    portENTER_CRITICAL(&accMux);
    sx = accX; sy = accY; sw = accWheel; rawBtns = latchBtns;
    accX = 0; accY = 0; accWheel = 0;
    portEXIT_CRITICAL(&accMux);

    int32_t dx = sx; if (dx > 127) dx = 127; else if (dx < -128) dx = -128;
    int32_t dy = sy; if (dy > 127) dy = 127; else if (dy < -128) dy = -128;
    int32_t remX = sx - dx, remY = sy - dy;
    if (remX || remY) { // 繰り越し（取りこぼさず、高速フリックは数フレームに分散）
        portENTER_CRITICAL(&accMux);
        accX += remX; accY += remY;
        portEXIT_CRITICAL(&accMux);
    }

    uint16_t gp_btns = rawBtns & 0x1F;
    if (sw > 0) gp_btns |= (1 << 8);
    else if (sw < 0) gp_btns |= (1 << 9);

    // 完全アイドル（動き無し・ボタン変化無し・直前も静止）なら送らずseqを止める
    static uint16_t lastBtns = 0;
    static bool lastMoving = false;
    bool moving = (dx != 0 || dy != 0 || sw != 0);
    if (!moving && gp_btns == lastBtns && !lastMoving) return;
    lastBtns = gp_btns;
    lastMoving = moving;

    packet_seq += 37; // emitごとに動かす（iPadはスティックで変化検知）
    dualsense.sendMouse(packet_seq, (uint8_t)(dx + 128), (uint8_t)(dy + 128), gp_btns);
}

void loop() {
#ifdef CALIB_MODE
    // 既知のテスト値 t を 0..255 でゆっくり掃引（計測用）。
    static uint8_t t = 0;
    static uint32_t last = 0;
    if (millis() - last >= 50) {
        last = millis();
        dualsense.sendCalib(t);
        t++;
    }
    delay(1);
#else
    if (!mouseConnected) {
        static uint32_t lastTry = 0;
        if (millis() - lastTry > 2000) {
            lastTry = millis();
            if (pClient == nullptr) {
                pClient = NimBLEDevice::createClient();
                pClient->setClientCallbacks(new MyClientCallbacks(), false);
            }
            if (!pClient->isConnected()) {
                if (pClient->connect(NimBLEAddress(TARGET_MOUSE_ADDR))) {
                    if (subscribeHID(pClient)) {
                        mouseConnected = true;
                    }
                }
            }
        }
        delay(20);
        return;
    }

    static uint32_t lastEmit = 0;
    if (millis() - lastEmit >= EMIT_MS) {
        lastEmit = millis();
        emitMouse();
    }
    delay(1);
#endif
}
