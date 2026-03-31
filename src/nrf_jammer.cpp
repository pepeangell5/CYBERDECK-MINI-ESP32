#include "nrf_jammer.h"
#include <RF24.h>
#include "display_config.h"
#include <esp_wifi.h> 

#ifndef MATRIX_GREEN
#define MATRIX_GREEN 0x07E0
#endif

// Pines del encoder
const int ENC_CLK_J = 48;
const int ENC_DT_J  = 46;
const int ENC_SW_J  = 3;

// Pines de botones (Basados en tu tabla de conexiones)
const int BTN_UP_J = 1;
const int BTN_DOWN_J = 2;

extern RF24 nrf1; 
extern RF24 nrf2; 

const byte jam_payload[] = { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 };

void forceStopRadios() {
    nrf1.stopConstCarrier();
    nrf1.powerDown(); 
    delay(10);
    nrf1.powerUp(); 
    nrf2.stopConstCarrier();
    nrf2.powerDown();
    delay(10);
    nrf2.powerUp();
    esp_wifi_start(); 
}

void drawJammerUI(LGFX &tft, int channel, bool active) {
    tft.fillScreen(TFT_BLACK);
    uint16_t frameCol = active ? TFT_RED : MATRIX_GREEN;
    
    tft.drawRect(5, 5, 310, 230, frameCol);
    tft.fillRect(10, 10, 300, 40, frameCol);
    
    tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(active ? 40 : 50, 22); 
    tft.print(active ? ">> JAMMING ACTIVE <<" : "NRF_JAMMER: IDLE");

    tft.setTextColor(TFT_WHITE); tft.setTextSize(4);
    tft.setCursor(45, 80); tft.printf("CH: %02d", channel);
    
    tft.setTextSize(1); tft.setTextColor(frameCol);
    tft.setCursor(45, 135); tft.printf("CENTER: 2.4%02d MHz", (channel * 5) + 7);
    tft.setCursor(45, 155); tft.print(active ? "[CLICK] TO STOP" : "[CLICK] TO ATTACK");
    tft.setCursor(45, 175); tft.print("[LONG CLICK] EXIT");
}

void showNRFJammer(LGFX &tft, bool &inSubMenu, int btnBack) {
    auto hardConfig = [](RF24 &r) {
        r.begin();
        r.setAddressWidth(3);
        r.setRetries(0, 0);
        r.setPALevel(RF24_PA_MAX, true);
        r.setDataRate(RF24_2MBPS);
        r.setAutoAck(false);
        r.setCRCLength(RF24_CRC_DISABLED);
        r.stopListening();
        r.openWritingPipe(0xFFFFFFFFFFLL);
    };

    hardConfig(nrf1);
    hardConfig(nrf2);

    int wifiChannel = 1;
    bool isAttacking = false;
    bool redraw = true;
    int lastClkJ = digitalRead(ENC_CLK_J);

    while (inSubMenu) {
        if (redraw) {
            drawJammerUI(tft, wifiChannel, isAttacking);
            redraw = false;
        }

        // --- ENCODER (Giro) ---
        int currentClk = digitalRead(ENC_CLK_J);
        if (!isAttacking && currentClk != lastClkJ && currentClk == LOW) {
            if (digitalRead(ENC_DT_J) != currentClk) {
                wifiChannel = (wifiChannel >= 14) ? 1 : wifiChannel + 1;
            } else {
                wifiChannel = (wifiChannel <= 1) ? 14 : wifiChannel - 1;
            }
            redraw = true;
        }
        lastClkJ = currentClk;

        // --- BOTONES UP / DOWN (Añadido) ---
        if (!isAttacking) {
            if (digitalRead(BTN_UP_J) == LOW) {
                wifiChannel = (wifiChannel <= 1) ? 14 : wifiChannel - 1;
                redraw = true;
                delay(150); // Debounce
            }
            if (digitalRead(BTN_DOWN_J) == LOW) {
                wifiChannel = (wifiChannel >= 14) ? 1 : wifiChannel + 1;
                redraw = true;
                delay(150); // Debounce
            }
        }

        // --- ENCODER (Click Corto/Largo) y BOTON ENTER (GPIO 42) ---
        if (digitalRead(ENC_SW_J) == LOW || digitalRead(42) == LOW) {
            unsigned long startP = millis();
            bool isLong = false;
            while(digitalRead(ENC_SW_J) == LOW || digitalRead(42) == LOW) {
                if (millis() - startP > 600) { isLong = true; break; }
                yield();
            }
            if (isLong) {
                isAttacking = false; forceStopRadios(); inSubMenu = false;
                delay(200);
            } else {
                isAttacking = !isAttacking;
                if (!isAttacking) forceStopRadios();
                redraw = true;
                delay(200);
            }
        }

        if (isAttacking) {
            esp_wifi_stop(); 
            int centerFreq = (wifiChannel * 5) + 7;

            // --- ANIMACIÓN DE RUIDO ---
            for(int a=0; a<5; a++) {
                int rx = random(10, 300);
                int rh = random(20, 100);
                tft.drawFastVLine(rx, 120 - (rh/2), rh, 0x4800); 
                tft.drawFastVLine(random(10,300), 50, 150, TFT_BLACK); 
            }

            // --- CICLO DE JAMMING ---
            for (int i = 0; i < 22; i++) {
                int target = centerFreq + i - 11;
                if (target < 0) target = 0;
                if (target > 125) target = 125;

                nrf1.setChannel(target);
                nrf2.setChannel(target + 1);

                nrf1.writeFast(&jam_payload, sizeof(jam_payload));
                nrf2.writeFast(&jam_payload, sizeof(jam_payload));
                
                if (digitalRead(ENC_SW_J) == LOW || digitalRead(42) == LOW) {
                    isAttacking = false; forceStopRadios(); redraw = true;
                    delay(500); break; 
                }
            }
        }

        if (digitalRead(btnBack) == LOW) { 
            forceStopRadios(); inSubMenu = false; delay(200); 
        }
        yield();
    }
}