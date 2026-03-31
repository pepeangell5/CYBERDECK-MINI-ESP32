#include "wifi_tool.h"
#include <math.h>

#define MATRIX_GREEN 0x07E0 

// Pines del encoder y botones
const int ENC_CLK_W = 48;
const int ENC_DT_W  = 46;
const int ENC_SW_W  = 3;
const int BTN_UP = 1;
const int BTN_DOWN = 2;

String target_ssid = "";
int target_channel = 1;

float calculateDistance(int rssi) {
    return pow(10, ((27.55 - (20 * log10(2412)) + abs(rssi)) / 20.0));
}

void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
}

// --- TECLADO VIRTUAL CON SCROLL ---
String virtualKeyboard(LGFX &tft) {
    String input = "";
    const char* keys = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%^&* ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int totalKeys = strlen(keys);
    int selKey = 0;
    int lastClkK = digitalRead(ENC_CLK_W);
    bool typing = true;

    auto drawKB = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(5, 5, 310, 230, MATRIX_GREEN);
        
        // Header del Teclado
        tft.fillRect(10, 10, 300, 30, MATRIX_GREEN);
        tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
        tft.setCursor(20, 17); tft.print("ENTER PASSWORD");
        
        // Área de texto (Input)
        tft.setTextColor(TFT_WHITE); tft.setTextSize(2);
        tft.setCursor(15, 55); 
        String displayPass = input;
        if(displayPass.length() > 18) displayPass = "..." + displayPass.substring(displayPass.length()-15);
        tft.print(displayPass + "_");

        // Lógica de Scroll de filas (Mostramos 3 filas de 10 teclas)
        int currentRow = selKey / 10;
        int startRow = 0;
        if (currentRow >= 3) startRow = currentRow - 2; // Scroll dinámico

        for(int i=0; i < 30; i++) { // Renderizamos máximo 30 teclas (3 filas)
            int keyIdx = (startRow * 10) + i;
            if (keyIdx >= totalKeys) break;

            int row = i / 10;
            int col = i % 10;
            int x = 20 + (col * 29);
            int y = 95 + (row * 30);

            if(keyIdx == selKey) {
                tft.fillRect(x-2, y-4, 25, 24, MATRIX_GREEN);
                tft.setTextColor(TFT_BLACK);
            } else {
                tft.drawRect(x-2, y-4, 25, 24, 0x3186); // Gris oscuro para rejilla
                tft.setTextColor(MATRIX_GREEN);
            }
            tft.setTextSize(2);
            tft.setCursor(x+4, y); tft.print(keys[keyIdx]);
        }

        // Footer limpio
        tft.drawFastHLine(10, 195, 300, MATRIX_GREEN);
        tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
        tft.setCursor(15, 205); tft.print("[CLICK] SELECT");
        tft.setCursor(15, 218); tft.print("[LONG CLICK] DONE / CONNECT");
    };

    drawKB();

    while(typing) {
        bool changed = false;
        
        // 1. Control por Encoder
        int currentClk = digitalRead(ENC_CLK_W);
        if (currentClk != lastClkK && currentClk == LOW) {
            if (digitalRead(ENC_DT_W) != currentClk) selKey = (selKey >= totalKeys - 1) ? 0 : selKey + 1;
            else selKey = (selKey <= 0) ? totalKeys - 1 : selKey - 1;
            changed = true;
        }
        lastClkK = currentClk;

        // 2. Control por Botones Físicos (1 y 2)
        if (digitalRead(BTN_UP) == LOW) { selKey = (selKey <= 0) ? totalKeys - 1 : selKey - 1; changed = true; delay(150); }
        if (digitalRead(BTN_DOWN) == LOW) { selKey = (selKey >= totalKeys - 1) ? 0 : selKey + 1; changed = true; delay(150); }

        if (changed) drawKB();

        // 3. Selección y Salida
        if (digitalRead(ENC_SW_W) == LOW) {
            unsigned long startP = millis();
            bool isLong = false;
            while(digitalRead(ENC_SW_W) == LOW) {
                if (millis() - startP > 700) { isLong = true; break; }
                yield();
            }
            if (isLong) { typing = false; } 
            else { 
                if (keys[selKey] == ' ') { 
                   input += " ";
                } else {
                   input += keys[selKey]; 
                }
                drawKB(); 
                delay(100); 
            }
        }
        yield();
    }
    return input;
}

void drawNetworkDetails(LGFX &tft, int index, int btnBack, int btnEnter) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, tft.width(), tft.height(), MATRIX_GREEN);
    tft.fillRect(0, 0, tft.width(), 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(10, 8); tft.print("ANALISIS_NODO");

    tft.setTextColor(MATRIX_GREEN); tft.setTextSize(1);
    tft.setCursor(15, 50); tft.printf("SSID: %s", WiFi.SSID(index).c_str());
    tft.setCursor(15, 70); tft.printf("CH: %d | RSSI: %d dBm", WiFi.channel(index), WiFi.RSSI(index));
    
    bool isOpen = (WiFi.encryptionType(index) == WIFI_AUTH_OPEN);
    
    tft.drawFastHLine(10, 160, tft.width()-20, MATRIX_GREEN);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(20, 175); tft.print("[CLICK] CONECTAR REAL");
    tft.setCursor(20, 195); tft.print("[LONG CLICK] CLONAR (EVIL)");
    tft.setCursor(20, 215); tft.print("[BACK] CANCELAR");

    while(true) {
        if (digitalRead(ENC_SW_W) == LOW || digitalRead(btnEnter) == LOW) {
            unsigned long pressTime = millis();
            while(digitalRead(ENC_SW_W) == LOW || digitalRead(btnEnter) == LOW) { yield(); }
            
            if (millis() - pressTime > 600) { 
                target_ssid = WiFi.SSID(index);
                target_channel = WiFi.channel(index);
                tft.fillRect(10, 100, 280, 40, MATRIX_GREEN);
                tft.setTextColor(TFT_BLACK); tft.setCursor(20, 115);
                tft.print("NODO CLONADO!"); delay(1000); break;
            } else { 
                String pass = "";
                if (!isOpen) { pass = virtualKeyboard(tft); }
                tft.fillScreen(TFT_BLACK);
                tft.setCursor(20, 100); tft.setTextColor(MATRIX_GREEN);
                tft.print("AUTENTICANDO...");
                WiFi.begin(WiFi.SSID(index).c_str(), pass.c_str());
                int retry = 0;
                while (WiFi.status() != WL_CONNECTED && retry < 20) { delay(500); retry++; }
                
                if (WiFi.status() == WL_CONNECTED) {
                    tft.fillScreen(TFT_BLACK); tft.setCursor(20, 100);
                    tft.print("SUCCESS! IP: "); tft.print(WiFi.localIP());
                    delay(3000);
                } else {
                    tft.setTextColor(TFT_RED); tft.setCursor(20, 120);
                    tft.print("AUTH_FAILED"); delay(2000);
                }
                break;
            }
        }
        if (digitalRead(btnBack) == LOW) { delay(200); break; }
        yield();
    }
}

void showWiFiScanner(LGFX &tft, bool &inSubMenu, int btnBack, int btnEnter) {
    int selectedNet = 0;
    int lastSelected = -1;
    int n = 0;
    int lastClkScanner = digitalRead(ENC_CLK_W);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(MATRIX_GREEN); tft.setTextSize(2);
    tft.setCursor(20, 100); tft.print("SCANNING...");
    n = WiFi.scanNetworks();

    while (inSubMenu) {
        if (selectedNet != lastSelected) {
            tft.fillScreen(TFT_BLACK);
            tft.drawRect(0, 0, tft.width(), tft.height(), MATRIX_GREEN);
            tft.fillRect(0, 0, tft.width(), 35, MATRIX_GREEN);
            tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
            tft.setCursor(10, 8); tft.print("NET_SCANNER");

            for (int i = 0; i < n && i < 5; i++) {
                int y = 55 + (i * 35);
                if (i == selectedNet) {
                    tft.fillRect(5, y - 5, tft.width() - 10, 30, MATRIX_GREEN);
                    tft.setTextColor(TFT_BLACK);
                } else {
                    tft.setTextColor(MATRIX_GREEN);
                }
                tft.setCursor(15, y); tft.printf("%s (%d)", WiFi.SSID(i).substring(0,15).c_str(), WiFi.RSSI(i));
            }
            lastSelected = selectedNet;
        }

        int currentClk = digitalRead(ENC_CLK_W);
        if (currentClk != lastClkScanner && currentClk == LOW) {
            if (digitalRead(ENC_DT_W) != currentClk) selectedNet = (selectedNet >= n - 1) ? 0 : selectedNet + 1;
            else selectedNet = (selectedNet <= 0) ? n - 1 : selectedNet - 1;
        }
        lastClkScanner = currentClk;

        if (digitalRead(BTN_UP) == LOW) { selectedNet = (selectedNet <= 0) ? n - 1 : selectedNet - 1; delay(150); }
        if (digitalRead(BTN_DOWN) == LOW) { selectedNet = (selectedNet >= n - 1) ? 0 : selectedNet + 1; delay(150); }

        if (digitalRead(ENC_SW_W) == LOW || digitalRead(btnEnter) == LOW) {
            unsigned long startTime = millis();
            bool isLong = false;
            while(digitalRead(ENC_SW_W) == LOW || digitalRead(btnEnter) == LOW) {
                if (millis() - startTime > 600) { isLong = true; break; }
                yield();
            }
            if (isLong) inSubMenu = false;
            else drawNetworkDetails(tft, selectedNet, btnBack, btnEnter);
            lastSelected = -1;
        }

        if (digitalRead(btnBack) == LOW) { inSubMenu = false; delay(200); }
        yield();
    }
    WiFi.scanDelete();
}