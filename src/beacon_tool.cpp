#include "beacon_tool.h"
#include "wifi_tool.h"
#include <esp_wifi.h>

#define MATRIX_GREEN 0x07E0 

const int ENC_SW_B = 3; 
bool isSpamming = false;

const char* ssids[] = {
    "👹Eres_Un_Pendejo", "💀Wifi_Para_Pendejos", "😈Wifi_Gratis", "📡Ey_Tu_La_De_Negro",
    "🔥Meteme_la_Vagina", "👾Te_Estoy_Viendo", "💣Enanos_y_caballos", "😱Porno_homosexual",
    "🤖Amlo_es_Puto", "🔞Puro_Morena_AMLO", "🧠Sheinbaum_se_la_come", "⚠️Puto_el_que_lo_lea",
    "🐍Conectate_y_te_hackeo", "💥El_Diablo_Te_Bendiga", "🔍Chupame_la_verga", "👽Paga_tu_internet", 
    "🔥Pinche_Pobre", "💾Putas_Gratis", "🚨Me_Cogi_a_tu_mama", "🖕Chupa_Limón_Kbron",
    "🤌Tu_Mama_Es_Hombre", "🤡Payaso_El_Que_Se_Conecte", "🧼Bañate_Cochino", "🕵️Cisen_Unidad_04",
    "👮Patrulla_Espacial_69", "🤮Tu_Cara_Da_Asquito", "🍄Vendo_Hongos_Alucinogenos", "🧨Cuidado_Explosivo",
    "🌚Me_Gustas_Cuando_Callas", "🥀Virgen_A_Los_40", "🍗Pollo_Frito_Gratis", "🍖Huele_A_Obito",
    "🦶Amo_Tus_Patas", "👅Lameme_El_Sipitajo", "🧟Zombi_En_Tu_Cochera", "👺Soy_Tu_Padre_HDP",
    "🍕Pizza_Con_Piña_Sux", "🌑Oscuro_Como_Tu_Conciencia", "💊Toma_Tu_Medicina", "📉Tu_IQ_Es_De_0"
};

uint8_t packet[128] = {
    0x80, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
    0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x64, 0x00, 0x11, 0x04, 
    0x00, 0x00 
};

void showBeaconSpammer(LGFX &tft, bool &inSubMenu, int btnBack) {
    isSpamming = false;

    auto drawFrame = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(5, 5, 310, 230, MATRIX_GREEN);
        tft.fillRect(10, 10, 300, 35, MATRIX_GREEN);
        tft.setTextColor(TFT_BLACK); tft.setTextSize(2);
        tft.setCursor(55, 17); tft.print("BEACON_SPAMMER");
        tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
        tft.setCursor(20, 205); tft.print("[CLICK] START/STOP");
        tft.setCursor(20, 220); tft.print("[LONG CLICK] EXIT TO MENU");
    };

    drawFrame();

    while (inSubMenu) {
        if (digitalRead(42) == LOW || digitalRead(ENC_SW_B) == LOW) {
            unsigned long pStart = millis();
            bool isLong = false;
            while(digitalRead(42) == LOW || digitalRead(ENC_SW_B) == LOW) {
                if (millis() - pStart > 600) { isLong = true; break; }
                yield();
            }
            if (isLong) { isSpamming = false; inSubMenu = false; }
            else {
                isSpamming = !isSpamming;
                if (isSpamming) {
                    esp_wifi_stop(); esp_wifi_deinit();
                    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                    esp_wifi_init(&cfg); 
                    esp_wifi_set_storage(WIFI_STORAGE_RAM);
                    esp_wifi_set_mode(WIFI_MODE_STA); 
                    esp_wifi_start();
                    esp_wifi_set_promiscuous(true);
                } else {
                    esp_wifi_set_promiscuous(false); esp_wifi_stop();
                    setupWiFi();
                }
                drawFrame();
            }
            delay(250);
        }

        if (isSpamming) {
            tft.setTextColor(TFT_RED); tft.setTextSize(3);
            tft.setCursor(75, 100); tft.print("INJECTING");

            // Animación de puntos
            for (int p = 0; p < 5; p++) {
                tft.drawPixel(random(10, 310), random(50, 195), MATRIX_GREEN);
            }

            // --- ESTRATEGIA DE INYECCIÓN PARA IPHONE ---
            for (int ch = 1; ch <= 11; ch++) {
                esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                
                // Enviamos los SSIDs que corresponden a este canal
                for (int i = 0; i < 40; i++) {
                    if ((i % 11) + 1 == ch) {
                        // MAC Única (Importante para iOS)
                        packet[10] = packet[16] = 0x22;
                        packet[11] = packet[17] = 0x33;
                        packet[12] = packet[18] = 0x44;
                        packet[13] = packet[19] = 0x55;
                        packet[14] = packet[20] = (uint8_t)(i >> 8);
                        packet[15] = packet[21] = (uint8_t)(i & 0xFF);

                        // Timestamp dinámico
                        uint64_t now = micros();
                        memcpy(&packet[24], &now, 8);

                        // SSID
                        int ssidLen = strlen(ssids[i]);
                        packet[37] = ssidLen;
                        for(int j=0; j<ssidLen; j++) packet[38+j] = ssids[i][j];

                        int p = 38 + ssidLen;
                        packet[p++] = 0x01; packet[p++] = 0x08; 
                        packet[p++] = 0x82; packet[p++] = 0x84; packet[p++] = 0x8b; packet[p++] = 0x96;
                        packet[p++] = 0x24; packet[p++] = 0x30; packet[p++] = 0x48; packet[p++] = 0x6c;
                        packet[p++] = 0x03; packet[p++] = 0x01;
                        packet[p++] = (uint8_t)ch;

                        esp_wifi_80211_tx(WIFI_IF_STA, packet, p, false);
                        // Pequeña ráfaga por cada red
                        delayMicroseconds(500); 
                    }
                }
                // El iPhone necesita que el canal esté "sucio" un rato para notar la red
                delay(2); 
            }
        } else {
            tft.setTextColor(MATRIX_GREEN); tft.setTextSize(3);
            tft.setCursor(85, 100); tft.print("STANDBY");
            tft.drawPixel(random(10, 310), random(50, 195), TFT_BLACK);
        }

        if (digitalRead(btnBack) == LOW) {
            isSpamming = false; inSubMenu = false;
            esp_wifi_set_promiscuous(false); esp_wifi_stop();
            setupWiFi(); delay(200);
        }
        yield();
    }
}