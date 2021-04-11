#include <Arduino.h>
#include <SPI.h>
#include <RH_NRF24.h>
#include <avr/power.h>
#include <avr/io.h>

#include <OTA.h>
#include <Sleep.h>
#include <EEPROM.h>

#include "common.h"

static RH_NRF24 RF(10, 9);

static constexpr uint16_t kMyUuid = 0xd959;

static constexpr uint8_t kBindButtonPin = 2;
static constexpr uint8_t kLampPins[] = {A0, A1/*, A2*/};
static constexpr uint8_t kNrfPowerPin = 4;
static constexpr uint8_t kPotPowerPin = 5;
static constexpr int kNoiseAmplitude = 6;
static constexpr int kRetransmitCount = 10;
static constexpr int kWdtPrescaler = 4;
static constexpr int kMinInterpacketDelay = 5;
static constexpr int kMaxInterpacketDelay = 15;

static int prev_vals[3];
static int counters[3];
static int out_mins[3];
static int out_maxes[3];
static OtaPacket pkt;

bool nrfPowered = false;

int AnalogNR(int pin) {
    digitalWrite(kPotPowerPin, HIGH);
    power_adc_enable();
    ADCSRA = (1 << ADEN);
    delayMicroseconds(10);
    int x = analogRead(pin);
    ADCSRA = 0;
    power_adc_disable();
    digitalWrite(kPotPowerPin, LOW);
    return x;
}

void SetRfEnabled(bool on) {
    if (on && !nrfPowered) {
        digitalWrite(kNrfPowerPin, HIGH);
        RF.init();
        RF.setChannel(1);
        RF.setRF(RH_NRF24::DataRate250kbps, RH_NRF24::TransmitPower0dBm);
        delay(15);
        nrfPowered = true;
    } else if (!on) {
        digitalWrite(kNrfPowerPin, LOW);
        nrfPowered = false;
    }
}

void setup() {
    PORTC = 0xfe;
    PORTD = 0xff;
    PORTB = 0x01;
    DIDR0 = 0xff;
    DIDR1 = 0xff;
    for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
        pinMode(kLampPins[i], INPUT);
        counters[i] = 0;
    }
    pinMode(kNrfPowerPin, OUTPUT);
    pinMode(kPotPowerPin, OUTPUT);
    pinMode(kBindButtonPin, INPUT_PULLUP);
    delay(30);

    if (digitalRead(kBindButtonPin) == LOW) {
        SetRfEnabled(true);
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
        }
        for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
            EepWrite16(i, out_mins[i]);
            EepWrite16(i + sizeof(out_mins), out_maxes[i]);
        }
    }
    for (uint8_t i = 0; i < sizeof(kLampPins); ++i) {
        out_mins[i] = EepRead16(i);
        out_maxes[i] = EepRead16(i + sizeof(out_mins));
    }
    SleepSetup(kWdtPrescaler);
    power_twi_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_usart0_disable();
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
            SetRfEnabled(true);
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
        SetRfEnabled(true);
        pkt.src_uuid = kMyUuid;
        pkt.light_id = 0xffff;
        pkt.light_brightness = 0;
        RF.send(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
        RF.waitPacketSent();
        RF.send(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
        RF.waitPacketSent();
    }

    if (allZeros) {
        RF.sleep();
        power_spi_disable();
        SetRfEnabled(false);
        SleepStart();
        power_spi_enable();
        power_timer0_enable();
    } else {
        delay(random(kMinInterpacketDelay, kMaxInterpacketDelay));
    }
}