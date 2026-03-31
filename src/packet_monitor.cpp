#include "packet_monitor.h"
#include <esp_wifi.h>
#include "display_config.h"

#define MATRIX_GREEN 0x07E0
#define GRAPH_HEIGHT 150 
#define GRAPH_BOTTOM 215 
#define HEADER_H 40

// Pines del encoder
const int ENC_CLK_P = 48;
const int ENC_DT_P  = 46;
const int ENC_SW_P  = 3;

extern LGFX tft;

volatile uint32_t count_all = 0;
volatile uint32_t count_data = 0;
uint32_t pkts_per_channel = 0;

unsigned long lastPktUpdate = 0;
unsigned long lastHopTime = 0;
bool autoHop = true;
int monitor_channel = 1;

LGFX_Sprite graphCanvas(&tft);

void sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
    count_all++;
    pkts_per_channel++;
    if (type == WIFI_PKT_DATA) count_data++;
}

void showPacketMonitor(LGFX &tft, bool &inSubMenu, int btnBack) {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
    
    if (!graphCanvas.createSprite(320, 200)) { inSubMenu = false; return; }
    graphCanvas.fillSprite(TFT_BLACK);
    
    pkts_per_channel = 0;
    count_all = 0;
    count_data = 0;
    int lastClkP = digitalRead(ENC_CLK_P);

    while (inSubMenu) {
        unsigned long now = millis();

        // 1. LÓGICA DE SALTOS DE CANAL (AUTO)
        if (autoHop && (now - lastHopTime > 1500)) {
            lastHopTime = now; 
            monitor_channel = (monitor_channel % 13) + 1;
            esp_wifi_set_channel(monitor_channel, WIFI_SECOND_CHAN_NONE);
            pkts_per_channel = 0; 
        }

        // 2. CONTROLES: ENCODER Y BOTONES
        
        // --- GIRO DEL ENCODER (Solo en modo Manual) ---
        int currentClk = digitalRead(ENC_CLK_P);
        if (!autoHop && currentClk != lastClkP && currentClk == LOW) {
            if (digitalRead(ENC_DT_P) != currentClk) {
                monitor_channel = (monitor_channel >= 13) ? 1 : monitor_channel + 1;
            } else {
                monitor_channel = (monitor_channel <= 1) ? 13 : monitor_channel - 1;
            }
            esp_wifi_set_channel(monitor_channel, WIFI_SECOND_CHAN_NONE);
            pkts_per_channel = 0;
        }
        lastClkP = currentClk;

        // --- BOTONES FÍSICOS (Manual) ---
        if (!autoHop) {
            if (digitalRead(1) == LOW) { // UP
                monitor_channel = (monitor_channel <= 1) ? 13 : monitor_channel - 1;
                esp_wifi_set_channel(monitor_channel, WIFI_SECOND_CHAN_NONE);
                pkts_per_channel = 0; delay(150);
            }
            if (digitalRead(2) == LOW) { // DOWN
                monitor_channel = (monitor_channel >= 13) ? 1 : monitor_channel + 1;
                esp_wifi_set_channel(monitor_channel, WIFI_SECOND_CHAN_NONE);
                pkts_per_channel = 0; delay(150);
            }
        }

        // --- CLIC: CORTO (Modo) / LARGO (Back) ---
        if (digitalRead(42) == LOW || digitalRead(ENC_SW_P) == LOW) {
            unsigned long startPress = millis();
            bool longPress = false;
            
            while(digitalRead(42) == LOW || digitalRead(ENC_SW_P) == LOW) {
                if (millis() - startPress > 600) { longPress = true; break; }
                yield();
            }

            if (longPress) {
                inSubMenu = false; // Salir
            } else {
                autoHop = !autoHop; // Cambiar modo SCAN/MAN
                delay(100);
            }
        }

        // 3. INTERFAZ Y GRÁFICA
        if (now - lastPktUpdate > 50) { 
            uint32_t snap_all = count_all;
            count_all = 0;
            count_data = 0;

            bool alert = (snap_all > 40);
            uint16_t headerCol = alert ? TFT_RED : (autoHop ? MATRIX_GREEN : 0x03E0);
            tft.fillRect(0, 0, 320, HEADER_H, headerCol);
            tft.setTextColor(TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.printf("CH:%02d PKTS:%u [%s]", monitor_channel, pkts_per_channel, autoHop ? "SCAN" : "MAN");

            graphCanvas.scroll(-1, 0); 
            graphCanvas.drawFastVLine(319, 0, 200, TFT_BLACK);

            int h = 0;
            if (snap_all > 0) {
                h = map(constrain(snap_all, 0, 30), 0, 30, 10, GRAPH_HEIGHT);
            }
            
            int localBottom = GRAPH_BOTTOM - HEADER_H;

            if (h > 0) {
                for (int y = 0; y < h; y++) {
                    uint16_t color;
                    if (y < 40)       color = MATRIX_GREEN;
                    else if (y < 90)  color = TFT_YELLOW;
                    else              color = TFT_RED;
                    graphCanvas.drawPixel(319, localBottom - y, color);
                }
            }
            
            graphCanvas.drawFastHLine(0, localBottom, 320, MATRIX_GREEN);
            graphCanvas.pushSprite(0, HEADER_H); 
            lastPktUpdate = now;
        }

        if (digitalRead(btnBack) == LOW) {
            inSubMenu = false;
            delay(200);
        }
        yield();
    }
    
    esp_wifi_set_promiscuous(false);
    graphCanvas.deleteSprite(); 
}