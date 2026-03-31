#include "gps_tool.h"
#include <TinyGPSPlus.h>
#include "display_config.h"
#include <math.h>

#define MATRIX_GREEN 0x07E0
#define PIN_BUZZER 14 

// Pines del encoder (basados en tu setup)
const int ENC_CLK_G = 48;
const int ENC_DT_G  = 46;
const int ENC_SW_G  = 3;

extern TinyGPSPlus gps;
extern HardwareSerial gpsSerial; 
extern LGFX tft; // Para el control de brillo

const int UTC_OFFSET = -7; // Sinaloa
bool mode24h = false;      // Alternar con clic corto
int screenBrightness = 180; // Brillo inicial

void drawCompass(LGFX &tft, float course) {
    int cx = 250; 
    int cy = 110; 
    int r = 35;   

    tft.drawCircle(cx, cy, r, MATRIX_GREEN);
    tft.setTextColor(MATRIX_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(cx - 3, cy - r - 10); tft.print("N");
    tft.setCursor(cx - 3, cy + r + 2);  tft.print("S");
    tft.setCursor(cx + r + 4, cy - 4);  tft.print("E");
    tft.setCursor(cx - r - 10, cy - 4); tft.print("W");

    tft.fillCircle(cx, cy, r - 2, TFT_BLACK);

    float rad = (course - 90) * 0.0174532925; 
    int lx = cx + cos(rad) * (r - 5);
    int ly = cy + sin(rad) * (r - 5);

    tft.drawLine(cx, cy, lx, ly, TFT_WHITE);
    tft.fillCircle(lx, ly, 3, 0xF800); 
}

void showGPSTool(LGFX &tft, bool &inSubMenu, int btnBack) {
    gpsSerial.begin(115200, SERIAL_8N1, 18, 17);
    
    bool fixAlertDone = false;
    unsigned long buzzerTimer = 0;
    int beepState = 0;
    int lastClkG = digitalRead(ENC_CLK_G);

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, tft.width(), tft.height(), MATRIX_GREEN);
    tft.fillRect(0, 0, tft.width(), 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(15, 8);
    tft.print("GPS_CYBER_TRACKER");

    while (inSubMenu) {
        while (gpsSerial.available() > 0) {
            gps.encode(gpsSerial.read());
        }

        // --- 1. LÓGICA DEL ENCODER (BRILLO) ---
        int currentClk = digitalRead(ENC_CLK_G);
        if (currentClk != lastClkG && currentClk == LOW) {
            if (digitalRead(ENC_DT_G) != currentClk) {
                screenBrightness = constrain(screenBrightness + 15, 10, 255);
            } else {
                screenBrightness = constrain(screenBrightness - 15, 10, 255);
            }
            tft.setBrightness(screenBrightness);
        }
        lastClkG = currentClk;

        // --- 2. CLIC CORTO (MODO HORA) / LARGO (BACK) ---
        if (digitalRead(ENC_SW_G) == LOW || digitalRead(42) == LOW) {
            unsigned long startP = millis();
            bool longP = false;
            while(digitalRead(ENC_SW_G) == LOW || digitalRead(42) == LOW) {
                if (millis() - startP > 600) { longP = true; break; }
                yield();
            }
            if (longP) {
                inSubMenu = false;
            } else {
                mode24h = !mode24h; // Cambiar formato de hora
                tft.fillRect(20, 170, 200, 30, TFT_BLACK); // Limpiar área de hora
                delay(100);
            }
        }

        // --- 3. LÓGICA DE PITIDO (FIX) ---
        if (gps.location.isValid() && !fixAlertDone) {
            if (beepState == 0) {
                digitalWrite(PIN_BUZZER, HIGH);
                buzzerTimer = millis();
                beepState = 1;
            }
            if (beepState == 1 && millis() - buzzerTimer > 40) {
                digitalWrite(PIN_BUZZER, LOW);
                if (millis() - buzzerTimer > 100) beepState = 2;
            }
            if (beepState == 2 && millis() - buzzerTimer > 140) {
                digitalWrite(PIN_BUZZER, LOW);
                fixAlertDone = true; 
            }
        }

        // --- 4. RENDERIZADO DE DATOS ---
        tft.setTextSize(1);
        tft.setCursor(20, 45);
        
        if (gps.location.isValid()) {
            tft.setTextColor(MATRIX_GREEN, TFT_BLACK);
            tft.printf("STATUS: [ FIXED ]  SATS: %d   ", gps.satellites.value());
            drawCompass(tft, gps.course.deg());
        } else {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.printf("STATUS: [ SCANNING... ] SATS: %d ", gps.satellites.value());
            fixAlertDone = false; beepState = 0;
            drawCompass(tft, 0);
        }

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(20, 70); tft.printf("LAT: %.6f   ", gps.location.lat());
        tft.setCursor(20, 85); tft.printf("LON: %.6f   ", gps.location.lng());
        tft.setCursor(20, 110); tft.printf("ALT: %.1f m  ", gps.altitude.meters());
        tft.setCursor(20, 125); tft.printf("VEL: %.1f km/h  ", gps.speed.kmph());

        tft.drawFastHLine(15, 150, 290, MATRIX_GREEN);
        
        tft.setCursor(20, 160); 
        tft.setTextColor(MATRIX_GREEN, TFT_BLACK);
        tft.printf("TIME (MOCHIS) - %s:", mode24h ? "24H" : "12H");
        
        if (gps.time.isValid()) {
            int h24 = gps.time.hour() + UTC_OFFSET;
            if (h24 < 0) h24 += 24;
            if (h24 >= 24) h24 -= 24;

            tft.setCursor(20, 175);
            tft.setTextSize(2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);

            if (mode24h) {
                tft.printf("%02d:%02d:%02d      ", h24, gps.time.minute(), gps.time.second());
            } else {
                const char* ampm = (h24 >= 12) ? "PM" : "AM";
                int h12 = h24 % 12;
                if (h12 == 0) h12 = 12;
                tft.printf("%02d:%02d:%02d %s  ", h12, gps.time.minute(), gps.time.second(), ampm);
            }
            tft.setTextSize(1);
        }

        tft.setCursor(20, 215);
        tft.setTextColor(0x7BEF); 
        tft.printf("HDOP: %.2f  BRIGHT: %d%%  ", gps.hdop.hdop(), (screenBrightness * 100) / 255);

        if (digitalRead(btnBack) == LOW) {
            inSubMenu = false;
            gpsSerial.end(); 
            delay(200);
        }
        yield();
    }
}