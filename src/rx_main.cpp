#include <Arduino.h>
#include <SPI.h>
#include <RH_NRF24.h>

#include <OTA.h>
#include <EEPROM.h>

#include "common.h"

static RH_NRF24 RF(10, 9);

static constexpr uint8_t kBindButtonPin = 2;
static constexpr uint8_t kRelayPins[] = {4,7,8};
static constexpr uint8_t kLampPins[] = {3,5,6};

static constexpr bool kEnableDebug = true; 

static OtaPacket pkt;
static uint8_t len = sizeof(pkt);

uint16_t MyMasterUuid = 0;

void bindMode() {
    bool bound = false;
    uint16_t prev_src_uuid = 0;
    while (!bound) {
        Serial.println("Waiting for bind packet ...");
        if (RF.waitAvailableTimeout(500)) {
            if (RF.recv(reinterpret_cast<uint8_t*>(&pkt), &len)) {
                if (pkt.light_id == 0xffff) {
                    if (prev_src_uuid == pkt.src_uuid) {
                        EepWrite16(0, pkt.src_uuid);
                        bound = true;
                        Serial.println("Got second bind packet. Done!");
                    }
                    prev_src_uuid = pkt.src_uuid;
                } else {
                    Serial.println("Got first bind packet ..");
                }
            }
        }
    }
}

void setup() {
    Serial.begin(9600);
    if (!RF.init())
        Serial.println("init failed");
    if (!RF.setChannel(1))
        Serial.println("setChannel failed");
    if (!RF.setRF(RH_NRF24::DataRate250kbps, RH_NRF24::TransmitPower0dBm))
        Serial.println("setRF failed");

    pinMode(kBindButtonPin, INPUT_PULLUP);
    for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
        pinMode(kLampPins[i], OUTPUT);
        pinMode(kRelayPins[i], OUTPUT);
    }
    delay(30);
    if (digitalRead(kBindButtonPin) == LOW) {
        bindMode();
    }
    MyMasterUuid = EepRead16(0);
}

void loop() {
    if (RF.available()) {
        if (RF.recv(reinterpret_cast<uint8_t*>(&pkt), &len)) {
            if (MyMasterUuid == pkt.src_uuid) {
                if (pkt.light_id < sizeof(kLampPins)) {
                    digitalWrite(kRelayPins[pkt.light_id], (pkt.light_brightness >> 8) > 1);
                    analogWrite(kLampPins[pkt.light_id], max(0,(pkt.light_brightness >> 8) - 2));
                    if (kEnableDebug) {
                        Serial.print(pkt.light_id);
                        Serial.print(" ");
                        Serial.print(pkt.src_uuid);
                        Serial.print(" ");
                        Serial.println(pkt.light_brightness >> 8);
                    }
                }
            }
        }
    }
}