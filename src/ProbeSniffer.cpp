#include "ProbeSniffer.h"

#include <WiFi.h>
#include "esp_wifi.h"

#include "DisplayTFT.h"
#include "PepeDraw.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

#define MAX_PROBES       80
#define MAX_CLIENTS      96
#define VISIBLE_ROWS     7
#define CHANNEL_HOP_MS   420
#define UI_REFRESH_MS    420

struct ClientEntry {
    uint8_t mac[6];
    uint16_t count;
    int8_t rssi;
    uint8_t lastChannel;
    uint32_t lastSeenMs;
};

enum SortMode {
    SORT_COUNT = 0,
    SORT_RECENT,
    SORT_RSSI,
    SORT_CLIENTS,
    SORT_COUNT_MODES
};

static ProbeEntry probes[MAX_PROBES];
static ClientEntry clients[MAX_CLIENTS];
static uint32_t probeClientMasks[MAX_PROBES];

static volatile int probeCount = 0;
static volatile int clientCount = 0;
static volatile uint32_t totalProbesCaptured = 0;
static volatile uint32_t broadcastProbeCount = 0;
static volatile uint32_t overflowCount = 0;
static volatile uint32_t newSsidTick = 0;
static volatile int currentChannel = 1;

static const int hopChannels[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
static constexpr int HOP_COUNT = sizeof(hopChannels) / sizeof(hopChannels[0]);
static int hopIdx = 0;
static SortMode sortMode = SORT_COUNT;
static bool paused = false;
static int g_cursor = 0;
static int g_scrollOffset = 0;
static bool emptyListDrawn = false;

static bool macEqual(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
}

static bool isRandomizedMac(const uint8_t* mac) {
    return (mac[0] & 0x02) != 0;
}

static uint32_t macHashBit(const uint8_t* mac) {
    uint8_t h = mac[0] ^ mac[1] ^ mac[2] ^ mac[3] ^ mac[4] ^ mac[5];
    return 1UL << (h & 31);
}

static void macShort(const uint8_t* mac, char* out, size_t outLen) {
    snprintf(out, outLen, "%02X:%02X:%02X",
             mac[3], mac[4], mac[5]);
}

static int findClient(const uint8_t* mac) {
    for (int i = 0; i < clientCount; i++) {
        if (macEqual(clients[i].mac, mac)) return i;
    }
    return -1;
}

static int addOrUpdateClient(const uint8_t* mac, int8_t rssi, uint8_t ch, uint32_t now) {
    int idx = findClient(mac);
    if (idx >= 0) {
        clients[idx].count++;
        clients[idx].rssi = rssi;
        clients[idx].lastChannel = ch;
        clients[idx].lastSeenMs = now;
        return idx;
    }

    if (clientCount >= MAX_CLIENTS) return -1;

    idx = clientCount++;
    memcpy(clients[idx].mac, mac, 6);
    clients[idx].count = 1;
    clients[idx].rssi = rssi;
    clients[idx].lastChannel = ch;
    clients[idx].lastSeenMs = now;
    return idx;
}

static bool ssidPrintable(const char* ssid, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t c = (uint8_t)ssid[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

static bool extractSsid(const uint8_t* payload, int len, char* out, uint8_t& outLen) {
    int pos = 24;
    while (pos + 2 <= len) {
        uint8_t tagId = payload[pos];
        uint8_t tagLen = payload[pos + 1];
        pos += 2;

        if (pos + tagLen > len) return false;

        if (tagId == 0x00) {
            outLen = tagLen;
            if (tagLen == 0) {
                strcpy(out, "<broadcast>");
                return true;
            }
            if (tagLen > 32) return false;
            memcpy(out, &payload[pos], tagLen);
            out[tagLen] = '\0';
            return ssidPrintable(out, tagLen);
        }

        pos += tagLen;
    }

    return false;
}

static void updateProbe(const char* ssid, const uint8_t* mac, int8_t rssi,
                        uint8_t channel, uint32_t now) {
    bool isBroadcast = strcmp(ssid, "<broadcast>") == 0;
    if (isBroadcast) broadcastProbeCount++;

    for (int i = 0; i < probeCount; i++) {
        if (strcmp(probes[i].ssid, ssid) == 0) {
            probes[i].count++;
            probes[i].rssi = rssi;
            probes[i].lastChannel = channel;
            probes[i].lastSeenMs = now;
            memcpy(probes[i].lastMac, mac, 6);

            uint32_t bit = macHashBit(mac);
            if ((probeClientMasks[i] & bit) == 0) {
                probeClientMasks[i] |= bit;
                if (probes[i].clients < 65535) probes[i].clients++;
            }
            return;
        }
    }

    if (probeCount >= MAX_PROBES) {
        overflowCount++;
        return;
    }

    int idx = probeCount++;
    strncpy(probes[idx].ssid, ssid, 32);
    probes[idx].ssid[32] = '\0';
    probes[idx].count = 1;
    probes[idx].clients = 1;
    probes[idx].rssi = rssi;
    probes[idx].firstChannel = channel;
    probes[idx].lastChannel = channel;
    probes[idx].lastSeenMs = now;
    memcpy(probes[idx].lastMac, mac, 6);
    probeClientMasks[idx] = macHashBit(mac);
    newSsidTick++;
}

static void probeSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (paused || type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    if (len < 26) return;
    if ((payload[0] & 0xFC) != 0x40) return;

    char ssid[33];
    uint8_t ssidLen = 0;
    if (!extractSsid(payload, len, ssid, ssidLen)) return;

    const uint8_t* srcMac = &payload[10];
    int8_t rssi = pkt->rx_ctrl.rssi;
    uint8_t channel = (uint8_t)currentChannel;
    uint32_t now = millis();

    totalProbesCaptured++;
    addOrUpdateClient(srcMac, rssi, channel, now);
    updateProbe(ssid, srcMac, rssi, channel, now);
}

int probeSnifferGetCount() {
    return probeCount;
}

bool probeSnifferGet(int idx, ProbeEntry& out) {
    if (idx < 0 || idx >= probeCount) return false;
    out = probes[idx];
    return true;
}

static int rssiBars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -85) return 2;
    if (rssi >= -95) return 1;
    return 0;
}

static const char* sortModeLabel() {
    switch (sortMode) {
        case SORT_RECENT:  return "REC";
        case SORT_RSSI:    return "RSSI";
        case SORT_CLIENTS: return "DEV";
        case SORT_COUNT:
        default:           return "CNT";
    }
}

static bool probeComesBefore(const ProbeEntry& a, const ProbeEntry& b) {
    switch (sortMode) {
        case SORT_RECENT:
            return a.lastSeenMs > b.lastSeenMs;
        case SORT_RSSI:
            return a.rssi > b.rssi;
        case SORT_CLIENTS:
            if (a.clients != b.clients) return a.clients > b.clients;
            return a.count > b.count;
        case SORT_COUNT:
        default:
            if (a.count != b.count) return a.count > b.count;
            return a.lastSeenMs > b.lastSeenMs;
    }
}

static void sortProbes() {
    int n = probeCount;
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (!probeComesBefore(probes[j], probes[j + 1])) {
                ProbeEntry tmp = probes[j];
                probes[j] = probes[j + 1];
                probes[j + 1] = tmp;
            }
        }
    }
}

static void clampCursor() {
    int total = probeCount;
    if (total <= 0) {
        g_cursor = 0;
        g_scrollOffset = 0;
        return;
    }
    if (g_cursor >= total) g_cursor = total - 1;
    if (g_cursor < 0) g_cursor = 0;
    if (g_cursor < g_scrollOffset) g_scrollOffset = g_cursor;
    if (g_cursor >= g_scrollOffset + VISIBLE_ROWS) {
        g_scrollOffset = g_cursor - VISIBLE_ROWS + 1;
    }
}

static void drawHeader() {
    tft.fillRect(0, 0, 320, 32, TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(8, 7, "PROBE SNIFFER", UI_MAIN, 1);

    char buf[36];
    snprintf(buf, sizeof(buf), "CH:%02d %s", currentChannel,
             paused ? "PAUSE" : "LIVE");
    drawStringCustom(210, 6, String(buf), paused ? TFT_YELLOW : TFT_GREEN, 1);

    snprintf(buf, sizeof(buf), "P:%lu D:%d B:%lu",
             (unsigned long)totalProbesCaptured,
             (int)clientCount,
             (unsigned long)broadcastProbeCount);
    drawStringCustom(210, 18, String(buf), UI_ACCENT, 1);
    tft.drawFastHLine(0, 32, 320, UI_ACCENT);
}

static void drawBars(int x, int y, int bars, bool selected) {
    for (int b = 0; b < 4; b++) {
        int bh = 3 + b * 2;
        uint16_t c = (b < bars)
            ? (selected ? UI_BG : (bars >= 3 ? TFT_GREEN :
                                  bars >= 2 ? TFT_YELLOW : TFT_ORANGE))
            : (selected ? UI_BG : UI_ACCENT);
        if (b < bars) tft.fillRect(x + b * 5, y - bh, 3, bh, c);
        else tft.drawRect(x + b * 5, y - bh, 3, bh, c);
    }
}

static void drawList() {
    const int rowH = 24;
    const int listY = 38;
    const int trackH = rowH * VISIBLE_ROWS;

    int total = probeCount;
    if (total == 0) {
        if (!emptyListDrawn) {
            tft.fillRect(2, listY, 316, trackH, TFT_BLACK);
            drawStringCustom(34, 88, "Esperando probe requests...", UI_ACCENT, 1);
            drawStringCustom(28, 106, "Gira encoder para navegar cuando aparezcan", UI_ACCENT, 1);
            drawStringCustom(48, 124, "BACK sale  |  OK cambia orden", UI_ACCENT, 1);
            emptyListDrawn = true;
        }
        return;
    }

    emptyListDrawn = false;
    clampCursor();
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = i + g_scrollOffset;
        int y = listY + i * rowH;
        tft.fillRect(5, y, 308, rowH - 2, TFT_BLACK);
        if (idx >= total) continue;

        ProbeEntry& p = probes[idx];
        bool sel = idx == g_cursor;
        uint16_t fg = sel ? UI_BG : UI_MAIN;
        uint16_t sub = sel ? UI_BG : UI_ACCENT;

        if (sel) tft.fillRect(5, y, 308, rowH - 2, UI_SELECT);
        else tft.drawFastHLine(8, y + rowH - 3, 302, 0x3186);

        String label = String(p.ssid);
        if (label == "<broadcast>") label = "<broadcast/hidden>";
        drawStringFit(10, y + 3, label, fg, 202, 1);

        char macBuf[12];
        macShort(p.lastMac, macBuf, sizeof(macBuf));

        unsigned long ago = (millis() - p.lastSeenMs) / 1000;
        char meta[64];
        snprintf(meta, sizeof(meta), "x%d dev:%d %ddBm ch%d %lus %s%s",
                 p.count, p.clients, p.rssi, p.lastChannel, ago,
                 isRandomizedMac(p.lastMac) ? "R " : "",
                 macBuf);
        drawStringFit(10, y + 14, String(meta), sub, 270, 1);
        drawBars(286, y + 17, rssiBars(p.rssi), sel);
    }

    if (total > VISIBLE_ROWS) {
        int barH = (VISIBLE_ROWS * trackH) / total;
        if (barH < 8) barH = 8;
        int barY = listY + (g_scrollOffset * (trackH - barH)) / (total - VISIBLE_ROWS);
        tft.fillRect(314, listY, 3, trackH, TFT_BLACK);
        tft.fillRect(314, barY, 3, barH, UI_ACCENT);
    } else {
        tft.fillRect(314, listY, 3, trackH, TFT_BLACK);
    }
}

static void drawFooter() {
    tft.fillRect(1, 211, 318, 28, TFT_BLACK);
    tft.drawFastHLine(0, 211, 320, UI_ACCENT);

    char left[36];
    snprintf(left, sizeof(left), "SSID:%d/%d OVF:%lu",
             (int)probeCount, MAX_PROBES, (unsigned long)overflowCount);
    drawStringCustom(8, 218, String(left), UI_MAIN, 1);

    String right = "OK:" + String(sortModeLabel()) + "  UP/DN/ENC  BACK";
    drawStringRight(312, 218, right, UI_ACCENT, 1);
}

static void nextSortMode() {
    sortMode = (SortMode)((sortMode + 1) % SORT_COUNT_MODES);
    sortProbes();
    g_cursor = 0;
    g_scrollOffset = 0;
    tft.startWrite();
    drawList();
    drawFooter();
    tft.endWrite();
}

static void moveCursor(int delta) {
    int total = probeCount;
    if (total <= 0) return;

    g_cursor = (g_cursor + total + delta) % total;
    clampCursor();
    beep(2100, 12);
    tft.startWrite();
    drawList();
    tft.endWrite();
}

static void runSnifferLoop() {
    drawHeader();
    drawList();
    drawFooter();

    unsigned long lastUI = millis();
    unsigned long lastHop = millis();
    unsigned long lastBtn = 0;
    uint32_t lastNewSsidTick = newSsidTick;
    bool stop = false;
    bool okWasDown = false;
    unsigned long okStart = 0;

    while (!stop) {
        if (!paused && millis() - lastHop > CHANNEL_HOP_MS) {
            hopIdx = (hopIdx + 1) % HOP_COUNT;
            currentChannel = hopChannels[hopIdx];
            esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
            lastHop = millis();
        }

        if (newSsidTick != lastNewSsidTick) {
            lastNewSsidTick = newSsidTick;
            beep(2800, 10);
        }

        if (millis() - lastUI > UI_REFRESH_MS) {
            sortProbes();
            clampCursor();
            tft.startWrite();
            drawHeader();
            drawList();
            drawFooter();
            tft.endWrite();
            lastUI = millis();
        }

        if (navBackPressed()) {
            stop = true;
        }

        if (navUpPressed() && millis() - lastBtn > 120) {
            moveCursor(-1);
            lastBtn = millis();
        }

        if (navDownPressed() && millis() - lastBtn > 120) {
            moveCursor(1);
            lastBtn = millis();
        }

        if (navEnterPressed()) {
            if (!okWasDown) {
                okWasDown = true;
                okStart = millis();
            } else if (millis() - okStart > 650) {
                paused = !paused;
                beep(paused ? 900 : 2400, 45);
                while (navEnterPressed()) delay(5);
                okWasDown = false;
                tft.startWrite();
                drawHeader();
                drawFooter();
                tft.endWrite();
            }
        } else if (okWasDown) {
            if (millis() - okStart < 650) {
                nextSortMode();
                beep(2400, 24);
            }
            okWasDown = false;
        }

        delay(8);
    }
}

void runProbeSniffer() {
    while (navEnterPressed() || navBackPressed()) delay(5);
    delay(80);

    memset(probes, 0, sizeof(probes));
    memset(clients, 0, sizeof(clients));
    memset(probeClientMasks, 0, sizeof(probeClientMasks));
    probeCount = 0;
    clientCount = 0;
    totalProbesCaptured = 0;
    broadcastProbeCount = 0;
    overflowCount = 0;
    newSsidTick = 0;
    sortMode = SORT_COUNT;
    paused = false;
    g_cursor = 0;
    g_scrollOffset = 0;
    emptyListDrawn = false;
    hopIdx = 0;
    currentChannel = hopChannels[0];

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(52, 86, "PROBE SNIFFER", UI_SELECT, 1);
    drawStringCustom(44, 120, "Promiscuous mode + channel hop", UI_ACCENT, 1);
    drawStringCustom(54, 138, "BACK sale | OK ordena | HOLD pausa", UI_ACCENT, 1);
    delay(450);

    WiFi.mode(WIFI_MODE_NULL);
    delay(100);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    wifi_promiscuous_filter_t filter;
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&probeSnifferCallback);
    esp_wifi_set_promiscuous(true);

    beep(2000, 35);
    delay(18);
    beep(2500, 50);

    runSnifferLoop();

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_stop();
    esp_wifi_deinit();
    WiFi.mode(WIFI_OFF);

    beep(1800, 35);
    delay(18);
    beep(1200, 50);

    while (navEnterPressed() || navBackPressed()) delay(5);
    delay(120);
}
