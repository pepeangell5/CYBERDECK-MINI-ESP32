#include "about.h"

#ifndef MATRIX_GREEN
#define MATRIX_GREEN 0x07E0
#endif

// Pines del encoder (Ya definidos en tu hardware)
const int ENC_CLK_A = 48;
const int ENC_DT_A  = 46;
const int ENC_SW_A  = 3;

// Pines de botones (Basados en tu configuracion de main.cpp)
const int BTN_UP_A = 1;
const int BTN_DOWN_A = 2;

void showAbout(LGFX &tft, bool &inSubMenu, int btnBack) {
    int scrollY = 0;
    int lastClkA = digitalRead(ENC_CLK_A);
    bool redraw = true;

    const char* lines[] = {
        "--- SYSTEM INFO ---",
        "DEVICE: CYBERDECK-S3",
        "OS: PEPE-OS V2.1",
        "CHIP: ESP32-S3 N8R2",
        " ",
        "--- DEVELOPER ---",
        "ALIAS: PepeAngell",
        "ROLE: Software Engineer",
        "LOC: Sinaloa, Mexico",
        " ",
        "--- HARDWARE ---",
        "DISPLAY: ST7789 2.0\"",
        "RF: 2x NRF24L01+PA+LNA",
        "GPS: NEO-6M MODULE",
        "STORAGE: SD CARD FAT32",
        "INPUT: ROTARY ENCODER",
        " ",
        "--- REDES ---",
        "IG: PEPEANGELL",
        "IG2: PEPEANGELLL",
        "GITHUB: PEPEANGELL5",
        "MAIL: joseangelchavezfelix@gmail.com",
        " ",
        "2026 - OPEN SOURCE"
    };
    int totalLines = sizeof(lines) / sizeof(lines[0]);
    int maxScroll = (totalLines * 20) - 140;

    while (inSubMenu) {
        if (redraw) {
            tft.startWrite(); // Optimizamos para LovyanGFX
            tft.fillScreen(TFT_BLACK);
            tft.drawRect(5, 5, 310, 230, MATRIX_GREEN);
            tft.fillRect(10, 10, 300, 35, MATRIX_GREEN);
            tft.setTextColor(TFT_BLACK); 
            tft.setTextSize(2);
            tft.setCursor(65, 17); 
            tft.print("ABOUT_SYSTEM");

            tft.setTextSize(1);
            tft.setTextColor(MATRIX_GREEN);
            
            for (int i = 0; i < totalLines; i++) {
                int yPos = 60 + (i * 20) - scrollY;
                // Solo dibujamos lo que entra en la "ventana" del recuadro
                if (yPos > 45 && yPos < 210) {
                    tft.setCursor(25, yPos);
                    tft.print(lines[i]);
                }
            }

            tft.drawFastHLine(10, 215, 300, MATRIX_GREEN);
            tft.setTextColor(TFT_WHITE);
            tft.setCursor(20, 222);
            tft.print("[UP/DN] MOVE  [CLICK/BACK] EXIT");
            tft.endWrite();
            redraw = false;
        }

        // --- CONTROL POR ENCODER ---
        int currentClk = digitalRead(ENC_CLK_A);
        if (currentClk != lastClkA && currentClk == LOW) {
            if (digitalRead(ENC_DT_A) != currentClk) {
                scrollY = constrain(scrollY + 20, 0, maxScroll);
            } else {
                scrollY = constrain(scrollY - 20, 0, maxScroll);
            }
            redraw = true;
        }
        lastClkA = currentClk;

        // --- CONTROL POR BOTONES UP/DOWN ---
        if (digitalRead(BTN_UP_A) == LOW) {
            scrollY = constrain(scrollY - 20, 0, maxScroll);
            redraw = true;
            delay(150); // Debounce para no scrollear de golpe
        }
        
        if (digitalRead(BTN_DOWN_A) == LOW) {
            scrollY = constrain(scrollY + 20, 0, maxScroll);
            redraw = true;
            delay(150);
        }

        // --- SALIDA ---
        if (digitalRead(ENC_SW_A) == LOW || digitalRead(btnBack) == LOW) {
            inSubMenu = false;
            delay(250);
        }
        yield();
    }
}