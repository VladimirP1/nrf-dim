#include <Arduino.h>
#include <SPI.h>
#include <RH_NRF24.h>

#include <OTA.h>
#include <Sleep.h>
#include <EEPROM.h>

#include "common.h"

static RH_NRF24 RF(10, 9);

static constexpr uint8_t kMyUuid = 0x389d;

static constexpr uint8_t kBindButtonPin = 2;
static constexpr uint8_t kLampPins[] = {A0/*, A1, A2*/};
static constexpr int kNoiseAmplitude = 6;
static constexpr int kRetransmitCount = 10;
static constexpr int kWdtPrescaler = 4;
static constexpr int kMinInterpacketDelay = 5;
static constexpr int kMaxInterpacketDelay = 25;

static int prev_vals[3];
static int counters[3];
static int out_mins[3];
static int out_maxes[3];
static OtaPacket pkt;

int AnalogNR(int pin) {
    analogRead(pin);
    delay(1);
    return (analogRead(pin) + analogRead(pin)) / 2;
}

void setup() {
    Serial.begin(9600);
    if (!RF.init())
        Serial.println("init failed");
    if (!RF.setChannel(1))
        Serial.println("setChannel failed");
    if (!RF.setRF(RH_NRF24::DataRate250kbps, RH_NRF24::TransmitPower0dBm))
        Serial.println("setRF failed");

    for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
        pinMode(kLampPins[i], INPUT);
        counters[i] = 0;
    }
    pinMode(4, OUTPUT);
    pinMode(kBindButtonPin, INPUT_PULLUP);
    delay(30);
    if (digitalRead(kBindButtonPin) == LOW) {
        for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
            out_mins[i] = 0x7fff;
            out_maxes[i] = 0;
        }
        while (digitalRead(kBindButtonPin) == LOW) {
            for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
                int val = AnalogNR(kLampPins[i]);

                pkt.src_uuid = kMyUuid;
                pkt.light_id = i;
                pkt.light_brightness = val << 6;
                RF.send(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
                RF.waitPacketSent();

                out_mins[i] = min(val, out_mins[i]);
                out_maxes[i] = max(val, out_maxes[i]);
            }
            delay(100);
            Serial.println("[In range mode]");
        }
        for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
            EepWrite16(i, out_mins[i]);
            EepWrite16(i + sizeof(out_mins), out_maxes[i]);
            Serial.print((float)out_mins[i]);
            Serial.print(" ");
            Serial.println((float)out_maxes[i]);
        }
    }
    for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
        out_mins[i] = EepRead16(i);
        out_maxes[i] = EepRead16(i + sizeof(out_mins));
        Serial.print((float)out_mins[i]);
        Serial.print(" ");
        Serial.println((float)out_maxes[i]);
    }
    SleepSetup(kWdtPrescaler);
}

void loop() {
    bool allZeros = true;
    for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
        int val = AnalogNR(kLampPins[i]);
        if (abs(val - prev_vals[i]) > kNoiseAmplitude) {
            counters[i] = kRetransmitCount;
            prev_vals[i] = val;
        }
        if (counters[i]) {
            val = ((float)val / 1024.f) * (out_maxes[i] - out_mins[i]) + out_mins[i];
            pkt.src_uuid = kMyUuid;
            pkt.light_id = i;
            pkt.light_brightness = ((uint16_t)val) << 6;
            RF.send(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
            RF.waitPacketSent();

            allZeros = false;
            --counters[i];
        }
    }
    if (digitalRead(kBindButtonPin) == LOW) {
        pkt.src_uuid = kMyUuid;
        pkt.light_id = 0xffff;
        pkt.light_brightness = 0;
        RF.send(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
        RF.waitPacketSent();
        Serial.println("Bind packet sent");
    }

    if (allZeros) {
        digitalWrite(4,LOW);
        RF.sleep();
        SleepStart();
        digitalWrite(4,HIGH);
    } else {
        delay(random(kMinInterpacketDelay, kMaxInterpacketDelay));
    }
}