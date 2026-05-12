#pragma once

#include <Arduino.h>
#include "Input.h"

// Shared SPI bus: TFT + nRF24 #1 + nRF24 #2
#define SCK_PIN   12
#define MOSI_PIN  11
#define MISO_PIN  13

// nRF24 #1
#define NRF1_CE_PIN   4
#define NRF1_CSN_PIN  5

// nRF24 #2
#define NRF2_CE_PIN   6
#define NRF2_CSN_PIN  7

// Backwards-compatible aliases. Existing scanner code uses CE_PIN/CSN_PIN.
#define CE_PIN   NRF1_CE_PIN
#define CSN_PIN  NRF1_CSN_PIN

// TFT SPI display
#define TFT_CS_PIN   10
#define TFT_RST_PIN  14
#define TFT_DC_PIN   21
#define TFT_LED_PIN  -1

// MicroSD dedicated SPI bus
#define SD_SCK_PIN   36
#define SD_MOSI_PIN  35
#define SD_MISO_PIN  37
#define SD_CS_PIN    16

// GPS NEO-6M on UART1
#define GPS_RX_PIN   18
#define GPS_TX_PIN   17
#define GPS_BAUD     9600

// Buttons, wired to GND when pressed
#define BTN_UP    1
#define BTN_DOWN  2
#define BTN_OK    42
#define BTN_BACK  41

// Rotary encoder, wired to GND when active
#define ENC_CLK_PIN  40
#define ENC_DT_PIN   39
#define ENC_SW_PIN   38

// Battery ADC input
#define VBAT_ADC_PIN 9

#define OK_LONGPRESS_MS 650

static inline bool waitOkReleaseWasLong(unsigned long holdMs = OK_LONGPRESS_MS) {
    unsigned long start = millis();
    bool wasLong = false;
    while (digitalRead(BTN_OK) == LOW || digitalRead(ENC_SW_PIN) == LOW) {
        if (millis() - start >= holdMs) wasLong = true;
        delay(5);
    }
    if (digitalRead(BTN_BACK) == LOW) {
        while (digitalRead(BTN_BACK) == LOW) delay(5);
        return true;
    }
    return wasLong;
}

#define BUZZER_PIN 15
