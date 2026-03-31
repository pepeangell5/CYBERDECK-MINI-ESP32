#include "nrf_spectrogram.h"
#include <SPI.h>
#include <RF24.h>

extern RF24 nrf1; 

#ifndef MATRIX_GREEN
#define MATRIX_GREEN 0x07E0
#endif

// Pines del hardware
const int ENC_CLK_S = 48;
const int ENC_DT_S  = 46;
const int ENC_SW_S  = 3;
const int PIN_BUZZER_S = 14; 

// Función de estática mejorada para Buzzers Pasivos
void playRadioStatic(int height) {
    if (height > 10) {
        // Generamos una frecuencia aleatoria entre 1000Hz y 3000Hz
        // La duración es mínima (1ms) para no alentar el escaneo
        int freq = random(1000, 3500); 
        tone(PIN_BUZZER_S, freq, 1); 
    }
}

void showNRFSpectrogram(LGFX &tft, bool &inSubMenu, int btnBack) {
    // 1. REINICIO DE RADIO
    nrf1.begin();
    delay(20);
    nrf1.setAutoAck(false);
    nrf1.setPALevel(RF24_PA_MAX); 
    nrf1.setDataRate(RF24_2MBPS);
    nrf1.stopListening();
    
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(5, 5, 310, 230, MATRIX_GREEN);
    tft.fillRect(10, 10, 300, 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(45, 17); tft.print("NRF_SPECTRUM_V2");
    
    int values[125]; 
    memset(values, 0, sizeof(values));
    int fallSpeed = 3; 
    int lastClkS = digitalRead(ENC_CLK_S);

    while (inSubMenu) {
        // --- 1. LÓGICA DEL ENCODER ---
        int currentClk = digitalRead(ENC_CLK_S);
        if (currentClk != lastClkS) {
            if (currentClk == LOW) {
                if (digitalRead(ENC_DT_S) != currentClk) {
                    fallSpeed = constrain(fallSpeed + 1, 1, 15);
                } else {
                    fallSpeed = constrain(fallSpeed - 1, 1, 15);
                }
            }
            lastClkS = currentClk;
        }

        // --- 2. CLICK LARGO PARA SALIR ---
        if (digitalRead(ENC_SW_S) == LOW || digitalRead(42) == LOW) {
            unsigned long pStart = millis();
            bool isLong = false;
            while(digitalRead(ENC_SW_S) == LOW || digitalRead(42) == LOW) {
                if (millis() - pStart > 600) { isLong = true; break; }
                yield();
            }
            if (isLong) { inSubMenu = false; break; }
        }

        // --- 3. ESCANEO DE CANALES ---
        for (int i = 0; i < 125; i++) {
            nrf1.setChannel(i);
            nrf1.startListening();
            delayMicroseconds(130); 
            bool signalDetected = nrf1.testCarrier(); 
            nrf1.stopListening();

            if (signalDetected) {
                if (values[i] < 140) values[i] += 20; 
                // Ahora llamamos a tone() para asegurar sonido en buzzers pasivos
                playRadioStatic(values[i]);
            } else {
                if (values[i] > 0) values[i] -= fallSpeed; 
                if (values[i] < 0) values[i] = 0;
            }

            // --- DIBUJO ---
            int x = 30 + (i * 2);
            int y_base = 200;
            int h = values[i];

            tft.drawFastVLine(x, 60, 140, TFT_BLACK); // Limpiar columna
            
            if (h > 0) {
                uint16_t color = MATRIX_GREEN;
                if (h > 50) color = 0xFFE0;  // Amarillo
                if (h > 100) color = 0xF800; // Rojo
                tft.drawFastVLine(x, y_base - h, h, color);
            }
        }

        // UI Inferior 
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(130, 210); tft.printf("DECAY: %02d", fallSpeed);
        
        // Referencias fijas
        tft.setCursor(30, 210); tft.print("2.40 GHz");
        tft.setCursor(240, 210); tft.print("2.52 GHz");

        if (digitalRead(btnBack) == LOW) {
            inSubMenu = false;
            delay(200);
        }
        yield(); 
    }
    
    noTone(PIN_BUZZER_S); // Asegurarnos de apagar el sonido al salir
    nrf1.powerDown();
}