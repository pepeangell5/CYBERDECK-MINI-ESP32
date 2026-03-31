#include "nrf_tool.h"
#include <SPI.h>
#include <RF24.h>
#include "display_config.h"

// Colores para el gradiente
#define MATRIX_GREEN  0x07E0
#define MATRIX_YELLOW 0xFFE0
#define MATRIX_RED    0xF800

// Pines del encoder
const int ENC_CLK_N = 48;
const int ENC_DT_N  = 46;
const int ENC_SW_N  = 3;

RF24 nrf1(4, 5); // Scanner A
RF24 nrf2(6, 7); // Scanner B

void showNRFTool(LGFX &tft, bool &inSubMenu, int btnBack) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(MATRIX_GREEN);
    tft.setCursor(20, 50);
    tft.println("INICIALIZANDO GRADIENT_HEATMAP...");

    pinMode(5, OUTPUT); digitalWrite(5, HIGH); 
    pinMode(7, OUTPUT); digitalWrite(7, HIGH); 
    delay(50);

    if (!nrf1.begin() || !nrf2.begin()) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED); tft.setCursor(20, 80);
        tft.println("ERROR: NRF HARDWARE FAIL");
        while(digitalRead(btnBack) == HIGH) { yield(); }
        inSubMenu = false; return;
    }

    nrf1.setPALevel(RF24_PA_LOW); nrf1.setDataRate(RF24_2MBPS); nrf1.stopListening();
    nrf2.setPALevel(RF24_PA_LOW); nrf2.setDataRate(RF24_2MBPS); nrf2.stopListening();

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, tft.width(), tft.height(), MATRIX_GREEN);
    tft.fillRect(0, 0, tft.width(), 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(15, 8);
    tft.print("NRF_HEATMAP_SCANNER");

    const int startNrf = 12; 
    const int endNrf = 84;   
    const int screenStart = 10;
    
    auto getX = [&](int nrf_channel) {
        return (int)(screenStart + ( (float)(nrf_channel - startNrf) * 4.166f ));
    };

    int values[126] = {0};
    int decayValue = 10; // Sensibilidad de caída inicial
    int lastClkN = digitalRead(ENC_CLK_N);

    while (inSubMenu) {
        // --- 1. LÓGICA DEL ENCODER (AJUSTE DE DECAY) ---
        int currentClk = digitalRead(ENC_CLK_N);
        if (currentClk != lastClkN && currentClk == LOW) {
            if (digitalRead(ENC_DT_N) != currentClk) {
                decayValue = constrain(decayValue + 2, 2, 30);
            } else {
                decayValue = constrain(decayValue - 2, 2, 30);
            }
        }
        lastClkN = currentClk;

        // --- 2. CLIC LARGO (BACK) / CLIC CORTO (RESET MAX) ---
        if (digitalRead(ENC_SW_N) == LOW || digitalRead(42) == LOW) {
            unsigned long pStart = millis();
            bool isLong = false;
            while(digitalRead(ENC_SW_N) == LOW || digitalRead(42) == LOW) {
                if (millis() - pStart > 600) { isLong = true; break; }
                yield();
            }
            if (isLong) {
                inSubMenu = false;
            } else {
                // Click corto: Limpia el heatmap de golpe
                memset(values, 0, sizeof(values));
                tft.fillRect(5, 40, 310, 155, TFT_BLACK); 
                delay(100);
            }
        }

        // Escaneo Dual Paralelo
        for (int i = startNrf; i < endNrf; i += 2) {
            int ch1 = i;
            int ch2 = i + 1;

            nrf1.setChannel(ch1); nrf1.startListening();
            delayMicroseconds(120); nrf1.stopListening();
            if (nrf1.testCarrier()) values[ch1] = min(values[ch1] + 25, 145);
            else values[ch1] = max(values[ch1] - decayValue, 0);

            nrf2.setChannel(ch2); nrf2.startListening();
            delayMicroseconds(120); nrf2.stopListening();
            if (nrf2.testCarrier()) values[ch2] = min(values[ch2] + 25, 145);
            else values[ch2] = max(values[ch2] - decayValue, 0);

            int x1 = getX(ch1);
            int x2 = getX(ch2);
            int barWidth = 4;

            tft.fillRect(x1, 45, 10, 145, TFT_BLACK); 

            auto getHeatColor = [](int val) {
                if (val > 100) return MATRIX_RED;
                if (val > 50)  return MATRIX_YELLOW;
                return MATRIX_GREEN;
            };

            tft.fillRect(x1, 190 - values[ch1], barWidth, values[ch1], getHeatColor(values[ch1]));
            tft.fillRect(x2, 190 - values[ch2], barWidth, values[ch2], getHeatColor(values[ch2]));
        }

        // Etiquetas de canal
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        int wifiCH[] = {1, 3, 6, 9, 11, 13, 14};
        int wifiIdx[] = {12, 22, 37, 52, 62, 72, 84};
        for(int i=0; i<7; i++) {
            int xP = getX(wifiIdx[i]);
            tft.drawFastVLine(xP, 192, 4, TFT_WHITE);
            tft.setCursor(xP - 4, 202);
            tft.print(wifiCH[i]);
        }

        tft.fillRect(10, 215, 300, 20, TFT_BLACK); 
        tft.setCursor(10, 220);
        tft.setTextColor(MATRIX_GREEN);
        tft.printf("DECAY: %02d | SCANNER: DUAL_NRF", decayValue);

        if (digitalRead(btnBack) == LOW) {
            inSubMenu = false;
            delay(200);
        }
        yield();
    }
}