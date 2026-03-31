#include <Arduino.h>
#include <TinyGPSPlus.h> 
#include "display_config.h"
#include "wifi_tool.h"
#include "beacon_tool.h" 
#include "evil_portal.h" 
#include "log_manager.h"
#include "packet_monitor.h" 
#include "gps_tool.h"    
#include "nrf_tool.h"    
#include "nrf_jammer.h"  
#include "nrf_spectrogram.h"
#include "file_explorer.h" 
#include "system_status.h"
#include "about.h"
#include "menu_icons.h"   
#include "boot_screen.h" 
#include <SPI.h>
#include "games_menu.h"
#include "ble_jammer.h"

extern SPIClass sdSPI; 

// --- PINES BOTONES ---
const int BTN_UP = 1, BTN_DOWN = 2, BTN_ENTER = 42, BTN_BACK = 41;  
const int PIN_BUZZER = 14; 

// --- PINES ENCODER ---
const int ENC_CLK = 48; 
const int ENC_DT  = 46; 
const int ENC_SW  = 3;  
const int BATT_PIN = 9; 

// Variables de estado
int lastClk;
unsigned long lastBattUpdate = 0;

LGFX tft;
int menuIndex = 0;
// CORRECCIÓN: Subimos a 14 opciones para incluir BLE JAMMER
const int totalOptions = 14; 

TinyGPSPlus gps;
HardwareSerial gpsSerial(1); 

// CORRECCIÓN: Añadido "BLE JAMMER" al final de la lista
const char* options[] = { 
    "WIFI SCANNER", "BEACON SPAMMER", "EVIL PORTAL", "PACKET MONITOR", 
    "GPS TRACKER", "NRF ANALYZER", "NRF JAMMER", "NRF SPECTRUM", 
    "FILE EXPLORER", "VIEW LOGS", "SYSTEM STATUS", "GAMES ARCADE", 
    "BLE JAMMER","ABOUT SYSTEM"
};

void drawBatteryIcon() {
    analogSetAttenuation(ADC_11db);
    int raw = analogRead(BATT_PIN);
    float vPin = (raw / 4095.0) * 3.3;
    float vBat = vPin * 3.45; 
    if (vBat > 0.5) vBat += 0.05;

    int pct = map(vBat * 100, 340, 420, 0, 100);
    pct = constrain(pct, 0, 100);

    uint16_t colorStatus;
    if (pct > 50) colorStatus = 0x03E0;      
    else if (pct > 20) colorStatus = 0xC618;      
    else colorStatus = TFT_RED;     

    tft.fillRect(245, 15, 30, 10, MATRIX_GREEN);
    tft.setTextColor(colorStatus, MATRIX_GREEN);
    tft.setTextSize(1);
    tft.setCursor(245, 15);
    tft.printf("%d%%", pct);

    tft.drawRect(278, 14, 22, 12, TFT_BLACK);
    tft.fillRect(279, 15, 20, 10, MATRIX_GREEN); 
    tft.fillRect(280, 16, (pct * 18) / 100, 8, colorStatus);
    tft.fillRect(300, 17, 2, 6, TFT_BLACK); 
}

void menuBeep() {
    digitalWrite(PIN_BUZZER, HIGH); 
    delay(30); 
    digitalWrite(PIN_BUZZER, LOW);
}

void drawMenu() {
    tft.startWrite();
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(5, 5, 310, 230, MATRIX_GREEN);
    tft.drawRect(8, 8, 304, 224, 0x03E0); 
    tft.fillRect(10, 10, 300, 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(15, 17);
    tft.printf("PEPEANGELL >#%d/%d", menuIndex + 1, totalOptions);

    drawBatteryIcon();

    drawGlifoIcon(tft, 160, 110, menuIndex);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    int nameWidth = strlen(options[menuIndex]) * 12;
    tft.setCursor((320 - nameWidth) / 2, 165);
    tft.print(options[menuIndex]);

    tft.setTextColor(MATRIX_GREEN);
    tft.setTextSize(1);
    tft.setCursor(145, 55);  tft.print("  /\\ UP");
    tft.setCursor(145, 195); tft.print("  \\/ DOWN");

    for(int i = 0; i < totalOptions; i++) {
        int xPos = (320 / 2) - (totalOptions * 10 / 2) + (i * 10);
        if (i == menuIndex) tft.fillCircle(xPos, 218, 3, MATRIX_GREEN);
        else tft.drawCircle(xPos, 218, 2, 0x03E0);
    }
    tft.endWrite();
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    tft.init(); 
    tft.setRotation(3); 
    tft.setBrightness(180);
    
    pinMode(BTN_UP, INPUT_PULLUP); 
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP); 
    pinMode(BTN_BACK, INPUT_PULLUP);
    
    pinMode(ENC_CLK, INPUT_PULLUP);
    pinMode(ENC_DT, INPUT_PULLUP);
    pinMode(ENC_SW, INPUT_PULLUP);
    lastClk = digitalRead(ENC_CLK);

    initLogSystem(); 
    drawBootScreen(tft); 
    setupWiFi();
    drawMenu();
}

void loop() {
    bool changed = false;

    if (millis() - lastBattUpdate > 30000) {
        drawBatteryIcon();
        lastBattUpdate = millis();
    }

    int currentClk = digitalRead(ENC_CLK);
    if (currentClk != lastClk && currentClk == LOW) {
        if (digitalRead(ENC_DT) != currentClk) {
            menuIndex = (menuIndex >= totalOptions - 1) ? 0 : menuIndex + 1;
        } else {
            menuIndex = (menuIndex <= 0) ? totalOptions - 1 : menuIndex - 1;
        }
        changed = true;
    }
    lastClk = currentClk;

    if (digitalRead(BTN_UP) == LOW) { 
        menuIndex = (menuIndex <= 0) ? totalOptions - 1 : menuIndex - 1;
        changed = true;
        delay(200); 
    }
    
    if (digitalRead(BTN_DOWN) == LOW) { 
        menuIndex = (menuIndex >= totalOptions - 1) ? 0 : menuIndex + 1;
        changed = true;
        delay(200); 
    }

    if (changed) {
        menuBeep();
        drawMenu();
    }
    
    if (digitalRead(BTN_ENTER) == LOW || digitalRead(ENC_SW) == LOW) {
        digitalWrite(PIN_BUZZER, HIGH); 
        delay(80); 
        digitalWrite(PIN_BUZZER, LOW);
        
        delay(250);
        bool inSubMenu = true;
        
        // --- LANZADOR DE HERRAMIENTAS ---
        if (menuIndex == 0)      showWiFiScanner(tft, inSubMenu, BTN_BACK, BTN_ENTER);
        else if (menuIndex == 1) showBeaconSpammer(tft, inSubMenu, BTN_BACK);
        else if (menuIndex == 2) showEvilPortal(tft, inSubMenu, BTN_BACK);
        else if (menuIndex == 3) showPacketMonitor(tft, inSubMenu, BTN_BACK);
        else if (menuIndex == 4) showGPSTool(tft, inSubMenu, BTN_BACK); 
        else if (menuIndex == 5) showNRFTool(tft, inSubMenu, BTN_BACK); 
        else if (menuIndex == 6) showNRFJammer(tft, inSubMenu, BTN_BACK); 
        else if (menuIndex == 7) showNRFSpectrogram(tft, inSubMenu, BTN_BACK); 
        else if (menuIndex == 8) showFileExplorer(tft, inSubMenu, BTN_BACK, BTN_UP, BTN_DOWN); 
        else if (menuIndex == 9) showLogsScreen(tft, BTN_BACK, BTN_ENTER); 
        else if (menuIndex == 10) showSystemStatus(tft, inSubMenu, BTN_BACK);
        else if (menuIndex == 11) showGamesMenu(tft, inSubMenu, BTN_BACK);
        else if (menuIndex == 12) launchBLEJammer(tft, inSubMenu, BTN_BACK);
        else if (menuIndex == 13) showAbout(tft, inSubMenu, BTN_BACK);
        
        drawMenu(); 
        delay(200);
    }
    yield();
}