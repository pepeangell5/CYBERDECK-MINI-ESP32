#include "file_explorer.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "display_config.h"

#define MATRIX_GREEN 0x07E0

// Pines del encoder
const int ENC_CLK_F = 48;
const int ENC_DT_F  = 46;
const int ENC_SW_F  = 3;

// Pines SD para S3 (SCK=12, MISO=13, MOSI=11, CS=10)
const int SD_CS = 10; 

// Ajustamos la firma para que coincida con lo que espera tu main.cpp
void showFileExplorer(LGFX &tft, bool &inSubMenu, int btnBack, int btnUp, int btnDown) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, tft.width(), tft.height(), MATRIX_GREEN);
    tft.fillRect(0, 0, tft.width(), 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(15, 8); tft.print("SD_FILE_EXPLORER");

    tft.setTextColor(MATRIX_GREEN); tft.setTextSize(1);
    tft.setCursor(20, 50); tft.println("MONTANDO SD CARD...");

    // --- INTENTO DE MONTAJE FORZADO ---
    SPI.begin(12, 13, 11, SD_CS); 
    delay(100);

    if (!SD.begin(SD_CS, SPI, 4000000)) { 
        tft.setTextColor(TFT_RED);
        tft.setCursor(20, 70); tft.println("ERROR: NO SE DETECTA SD");
        tft.setCursor(20, 90); tft.println("FAT32 REQUERIDO_");
        
        unsigned long errorStart = millis();
        while(millis() - errorStart < 3000) { // Esperar 3 seg o botón back
            if(digitalRead(btnBack) == LOW || digitalRead(ENC_SW_F) == LOW) break;
            yield();
        }
        inSubMenu = false; return;
    }

    tft.setTextColor(MATRIX_GREEN);
    tft.setCursor(20, 70); tft.println("SD OK! LEYENDO ARCHIVOS...");
    delay(300);

    File root = SD.open("/");
    int fileCount = 0;
    String fileNames[30]; 

    if (root) {
        File entry = root.openNextFile();
        while (entry && fileCount < 30) {
            if (!entry.isDirectory()) {
                fileNames[fileCount] = String(entry.name());
                fileCount++;
            }
            entry = root.openNextFile();
        }
        root.close();
    }

    int scrollIdx = 0;
    bool redraw = true;
    int lastClkF = digitalRead(ENC_CLK_F);

    while (inSubMenu) {
        if (redraw) {
            tft.fillRect(5, 40, 310, 170, TFT_BLACK); 
            tft.setTextColor(MATRIX_GREEN);
            
            if (fileCount == 0) {
                tft.setCursor(20, 100); tft.print("SD VACIA O NO LEIDA_");
            } else {
                for (int i = 0; i < 8; i++) { 
                    int idx = i + (scrollIdx / 8) * 8; 
                    if (idx < fileCount) {
                        int y = 55 + (i * 18);
                        if (idx == scrollIdx) {
                            tft.fillRect(10, y - 2, 300, 15, MATRIX_GREEN);
                            tft.setTextColor(TFT_BLACK);
                        } else {
                            tft.setTextColor(MATRIX_GREEN);
                        }
                        tft.setCursor(15, y); tft.print(fileNames[idx].substring(0, 35));
                    }
                }
            }
            
            tft.drawFastHLine(10, 215, 300, MATRIX_GREEN);
            tft.setTextColor(TFT_WHITE);
            tft.setCursor(10, 222); 
            tft.printf("FILES:%d | POS:%d", fileCount, (fileCount > 0 ? scrollIdx + 1 : 0));
            redraw = false;
        }

        // --- ENCODER SCROLL ---
        int currentClk = digitalRead(ENC_CLK_F);
        if (currentClk != lastClkF && currentClk == LOW) {
            if (digitalRead(ENC_DT_F) != currentClk) {
                if (scrollIdx < fileCount - 1) { scrollIdx++; redraw = true; }
            } else {
                if (scrollIdx > 0) { scrollIdx--; redraw = true; }
            }
        }
        lastClkF = currentClk;

        // --- BOTONES FISICOS (BACKUP) ---
        if (digitalRead(btnUp) == LOW) { if (scrollIdx > 0) { scrollIdx--; redraw = true; } delay(150); }
        if (digitalRead(btnDown) == LOW) { if (scrollIdx < fileCount - 1) { scrollIdx++; redraw = true; } delay(150); }

        // --- SALIR (LONG CLICK) ---
        if (digitalRead(ENC_SW_F) == LOW || digitalRead(btnBack) == LOW) {
            unsigned long pStart = millis();
            bool isLong = false;
            while(digitalRead(ENC_SW_F) == LOW || digitalRead(btnBack) == LOW) {
                if (millis() - pStart > 600) { isLong = true; break; }
                yield();
            }
            if (isLong || digitalRead(btnBack) == LOW) { inSubMenu = false; break; }
        }
        yield();
    }
    SD.end(); 
}