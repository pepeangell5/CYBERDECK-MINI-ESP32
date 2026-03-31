#include "system_status.h"
#include <WiFi.h>

#define MATRIX_GREEN 0x07E0
const int BATT_PIN = 9; 

float getBatteryVoltage() {
    analogSetAttenuation(ADC_11db);
    int raw = analogRead(BATT_PIN);
    float vPin = (raw / 4095.0) * 3.3;
    float vBat = vPin * 3.45;
    if (vBat > 0.5) vBat += 0.05; 
    return vBat;
}

void showSystemStatus(LGFX &tft, bool &inSubMenu, int btnBack) {
    // Dibujamos la interfaz estática UNA SOLA VEZ al entrar
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(5, 5, 310, 230, MATRIX_GREEN);
    tft.fillRect(10, 10, 300, 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(55, 17); tft.print("SYS_DIAGNOSTICS");
    
    tft.drawFastHLine(10, 215, 300, MATRIX_GREEN);
    tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
    tft.setCursor(20, 222); tft.print("[BACK] EXIT TO SHELL_");

    unsigned long lastUpdate = 0;

    while (inSubMenu) {
        if (millis() - lastUpdate > 1000) {
            tft.startWrite();
            
            tft.setTextColor(MATRIX_GREEN, TFT_BLACK); // Texto verde con fondo negro para sobreescribir
            tft.setTextSize(1);
            int y = 60;

            // --- 1. BATERÍA ---
            float vbat = getBatteryVoltage();
            int pct = map(vbat * 100, 340, 420, 0, 100);
            pct = constrain(pct, 0, 100);
            
            // Borramos solo la línea de la batería con un rectángulo negro pequeño
            tft.fillRect(20, y, 190, 10, TFT_BLACK); 
            tft.setCursor(20, y); tft.printf("BATTERY: %.2fV [%d%%]", vbat, pct);
            
            // Actualizar barra sin borrar el borde
            tft.drawRect(220, y-2, 70, 12, MATRIX_GREEN);
            tft.fillRect(221, y-1, 68, 10, TFT_BLACK); // Limpiar interior de la barra
            uint16_t barColor = (pct < 20) ? TFT_RED : MATRIX_GREEN;
            tft.fillRect(222, y, (pct * 66) / 100, 8, barColor);
            y += 30;

            // --- 2. RECURSOS ---
            tft.fillRect(20, y, 280, 10, TFT_BLACK);
            tft.setCursor(20, y); tft.printf("RAM FREE: %d KB", ESP.getFreeHeap() / 1024);
            y += 25;
            
            tft.fillRect(20, y, 280, 10, TFT_BLACK);
            tft.setCursor(20, y); tft.printf("CPU TEMP: %.1f C", temperatureRead());
            y += 25;

            // --- 3. UPTIME ---
            long sec = millis() / 1000;
            tft.fillRect(20, y, 280, 10, TFT_BLACK);
            tft.setCursor(20, y); tft.printf("UPTIME: %02d:%02d:%02d", (int)(sec/3600), (int)(sec/60)%60, (int)sec%60);
            y += 30;

            // --- 4. RED ---
            tft.fillRect(20, y, 280, 10, TFT_BLACK);
            tft.setCursor(20, y); 
            if(WiFi.status() == WL_CONNECTED) {
                tft.printf("NET: %s | IP: %s", WiFi.SSID().substring(0,10).c_str(), WiFi.localIP().toString().c_str());
            } else {
                tft.print("NET: DISCONNECTED");
            }
            
            tft.endWrite();
            lastUpdate = millis();
        }

        // Salir con Botón Back o Click de Encoder (Pin 3)
        if (digitalRead(btnBack) == LOW || digitalRead(3) == LOW) {
            inSubMenu = false;
            delay(200);
        }
        yield();
    }
}