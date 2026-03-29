#include <Arduino.h>
#define USE_NIMBLE
#include <NimBLEDevice.h>
#include <nvs_flash.h>

const char* TARGET_MOUSE_ADDR = "a8:10:87:18:f6:37";

static NimBLEClient* pClient = nullptr;
static bool mouseConnected = false;

// マウスからの生データを出力
void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print("Mouse Data: ");
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
}

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* _pClient) { Serial.println("Connected to Mouse."); }
    void onDisconnect(NimBLEClient* _pClient) { 
        Serial.println("Disconnected. Restarting...");
        delay(2000);
        ESP.restart();
    }
};

void setup() {
    Serial.begin(115200);
    nvs_flash_init();

    NimBLEDevice::init("ESP32-Mouse-Diagnostic");
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    Serial.println("Searching for Mouse...");

    while (!mouseConnected) {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        NimBLEScanResults results = pScan->start(5, false);

        for (int i = 0; i < results.getCount(); i++) {
            NimBLEAdvertisedDevice device = results.getDevice(i);
            if (device.getAddress().toString() == TARGET_MOUSE_ADDR) {
                Serial.println("Target Found! Connecting...");
                
                pClient = NimBLEDevice::createClient();
                pClient->setClientCallbacks(new ClientCallbacks(), false);

                if (pClient->connect(device.getAddress(), false)) {
                    delay(1000);
                    Serial.println("Securing Connection...");
                    pClient->secureConnection();
                    delay(1000);

                    // --- 自動探索ロジック開始 ---
                    NimBLERemoteService* pSvc = pClient->getService("1812");
                    if (pSvc) {
                        Serial.println("HID Service (1812) found. Scanning characteristics...");
                        
                        // サービス内のすべての Characteristic を取得
                        auto charList = pSvc->getCharacteristics(true);
                        bool subSuccess = false;

                        for (auto &pChr : *charList) {
                            Serial.printf(" - Found Characteristic: %s", pChr->getUUID().toString().c_str());
                            
                            // 2A4D (Report) または 2A33 (Boot Mouse) かつ Notify 可能なものを探す
                            if ((pChr->getUUID() == NimBLEUUID("2A4D") || pChr->getUUID() == NimBLEUUID("2A33")) && 
                                pChr->canNotify()) {
                                
                                if (pChr->subscribe(true, notifyCallback)) {
                                    Serial.println(" -> [SUBSCRIBED]");
                                    subSuccess = true;
                                    // 複数の Report がある場合、最初に見つかった移動データ用を優先
                                } else {
                                    Serial.println(" -> [Subscribe Failed]");
                                }
                            } else {
                                Serial.println("");
                            }
                        }

                        if (subSuccess) {
                            Serial.println("--- SUCCESS! Move the mouse ---");
                            mouseConnected = true;
                            break;
                        }
                    } else {
                        Serial.println("HID Service not found. Is this a HID mouse?");
                    }
                }
            }
        }
        pScan->clearResults();
        if (!mouseConnected) delay(1000);
    }
}

void loop() { delay(1000); }