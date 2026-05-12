#include "ThreatMonitor.h"

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

#include "DisplayTFT.h"
#include "Input.h"
#include "PepeDraw.h"
#include "PeripheralTools.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

#define TM_MAX_APS        36
#define TM_CHANNEL_MS     240
#define TM_DRAW_MS        500
#define TM_REPORT_PATH    "/THREAT_REPORT.txt"
#define TM_UNIQUE_TRACK   96

static const uint8_t TM_AUTO_CHANNELS[] = {
    1, 6, 11, 1, 6, 11,
    2, 3, 4, 5, 7, 8, 9, 10, 12, 13
};
static const int TM_AUTO_CHANNEL_COUNT = sizeof(TM_AUTO_CHANNELS) / sizeof(uint8_t);

struct TmAp {
    char ssid[33];
    char bssid[18];
    uint8_t channel;
    int8_t rssi;
    uint8_t auth;
};

static TmAp tmAps[TM_MAX_APS];
static int tmApCount = 0;
static int tmOpenCount = 0;
static int tmWeakCount = 0;
static int tmDuplicateSsidCount = 0;

static volatile uint32_t tmMgmtSec = 0;
static volatile uint32_t tmBeaconSec = 0;
static volatile uint32_t tmProbeSec = 0;
static volatile uint32_t tmDeauthSec = 0;
static volatile uint32_t tmDisassocSec = 0;
static volatile uint32_t tmBeaconTotal = 0;
static volatile uint32_t tmProbeTotal = 0;
static volatile uint32_t tmDeauthTotal = 0;
static volatile uint32_t tmDisassocTotal = 0;
static volatile uint32_t tmBeaconUniqueBssidSec = 0;
static volatile uint32_t tmBeaconUniqueSsidSec = 0;

static uint32_t tmLastMgmt = 0;
static uint32_t tmLastBeacon = 0;
static uint32_t tmLastProbe = 0;
static uint32_t tmLastDeauth = 0;
static uint32_t tmLastDisassoc = 0;
static uint32_t tmLastBeaconUniqueBssid = 0;
static uint32_t tmLastBeaconUniqueSsid = 0;
static int tmLiveRisk = 0;

static uint32_t tmBeaconBssidHashes[TM_UNIQUE_TRACK];
static uint32_t tmBeaconSsidHashes[TM_UNIQUE_TRACK];
static int tmBeaconBssidHashCount = 0;
static int tmBeaconSsidHashCount = 0;

static portMUX_TYPE tmCounterMux = portMUX_INITIALIZER_UNLOCKED;

static int tmChannel = 1;
static int tmAutoChannelIndex = 0;
static bool tmPaused = false;

static const char* authName(uint8_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "?";
    }
}

static bool isWeakAuth(uint8_t auth) {
    return auth == WIFI_AUTH_WEP || auth == WIFI_AUTH_WPA_PSK;
}

static String safeField(const String& in) {
    String out = in;
    out.replace("\r", " ");
    out.replace("\n", " ");
    out.replace(",", " ");
    return out;
}

static String maskedBssid(const char* bssid) {
    String b = String(bssid);
    if (b.length() < 17) return b;
    return b.substring(0, 8) + ":xx:xx:xx";
}

static uint32_t hashBytes(const uint8_t* data, int len) {
    uint32_t h = 2166136261UL;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619UL;
    }
    return h == 0 ? 1 : h;
}

static bool rememberHash(uint32_t* hashes, int& count, uint32_t h) {
    for (int i = 0; i < count; i++) {
        if (hashes[i] == h) return false;
    }

    if (count < TM_UNIQUE_TRACK) {
        hashes[count++] = h;
        return true;
    }

    return false;
}

static uint32_t beaconSsidHash(const uint8_t* p, int len) {
    int pos = 36;
    while (pos + 2 <= len) {
        uint8_t tag = p[pos];
        uint8_t tagLen = p[pos + 1];
        pos += 2;
        if (pos + tagLen > len) break;
        if (tag == 0) {
            if (tagLen == 0 || tagLen > 32) return 0;
            return hashBytes(p + pos, tagLen);
        }
        pos += tagLen;
    }
    return 0;
}

static int baselineRiskScore() {
    int score = 0;
    score += min(tmOpenCount * 6, 18);
    score += min(tmWeakCount * 10, 24);
    score += min(tmDuplicateSsidCount * 12, 24);
    if (score > 40) score = 40;
    return score;
}

static int instantEventRisk() {
    int score = 0;
    score += min((int)tmLastDeauth * 18, 60);
    score += min((int)tmLastDisassoc * 14, 45);

    if (tmLastBeaconUniqueBssid > 18) {
        score += min((int)(tmLastBeaconUniqueBssid - 18) * 2 + 24, 55);
    }
    if (tmLastBeaconUniqueSsid > 8) {
        score += min((int)(tmLastBeaconUniqueSsid - 8) * 3 + 18, 45);
    }
    if (tmLastBeacon > 55) {
        score += min((int)(tmLastBeacon - 55) / 3 + 12, 32);
    }
    if (tmLastMgmt > 260) {
        score += min((int)(tmLastMgmt - 260) / 10 + 12, 28);
    }

    if (score > 70) score = 70;
    return score;
}

static void updateLiveRisk() {
    int instant = instantEventRisk();
    if (instant >= tmLiveRisk) {
        tmLiveRisk = instant;
        return;
    }

    tmLiveRisk -= 18;
    if (tmLiveRisk < instant) tmLiveRisk = instant;
    if (tmLiveRisk < 0) tmLiveRisk = 0;
}

static int riskScore() {
    int score = baselineRiskScore() + tmLiveRisk;
    if (score > 100) score = 100;
    return score;
}

static const char* riskLabel(int score) {
    if (score < 20) return "LOW";
    if (score < 45) return "WATCH";
    if (score < 70) return "RISK";
    return "ALERT";
}

static uint16_t riskColor(int score) {
    if (score < 20) return TFT_GREEN;
    if (score < 45) return TFT_CYAN;
    if (score < 70) return TFT_YELLOW;
    return TFT_RED;
}

static void drawScanFrame(const char* msg) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(10, 8, "THREAT MON", TFT_WHITE, 1);
    tft.drawFastHLine(0, 34, 320, TFT_WHITE);
    drawStringCustom(28, 96, msg, TFT_CYAN, 2);
    drawStringCustom(28, 128, "Modo pasivo / defensivo", UI_ACCENT, 1);
}

static void scanBaseline() {
    drawScanFrame("Escaneando superficie...");
    memset(tmAps, 0, sizeof(tmAps));
    tmApCount = 0;
    tmOpenCount = 0;
    tmWeakCount = 0;
    tmDuplicateSsidCount = 0;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(120);

    int n = WiFi.scanNetworks(false, true);
    if (n < 0) n = 0;
    if (n > TM_MAX_APS) n = TM_MAX_APS;

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "<hidden>";
        ssid.toCharArray(tmAps[i].ssid, sizeof(tmAps[i].ssid));
        WiFi.BSSIDstr(i).toCharArray(tmAps[i].bssid, sizeof(tmAps[i].bssid));
        tmAps[i].channel = (uint8_t)WiFi.channel(i);
        tmAps[i].rssi = (int8_t)WiFi.RSSI(i);
        tmAps[i].auth = WiFi.encryptionType(i);
        if (tmAps[i].auth == WIFI_AUTH_OPEN) tmOpenCount++;
        if (isWeakAuth(tmAps[i].auth)) tmWeakCount++;
        tmApCount++;
    }

    for (int i = 0; i < tmApCount; i++) {
        if (strcmp(tmAps[i].ssid, "<hidden>") == 0) continue;
        bool alreadyCounted = false;
        for (int k = 0; k < i; k++) {
            if (strcmp(tmAps[k].ssid, tmAps[i].ssid) == 0) {
                alreadyCounted = true;
                break;
            }
        }
        if (alreadyCounted) continue;

        int unique = 0;
        for (int j = i; j < tmApCount; j++) {
            if (strcmp(tmAps[j].ssid, tmAps[i].ssid) == 0) unique++;
        }
        if (unique > 1) tmDuplicateSsidCount++;
    }

    WiFi.scanDelete();
}

static void threatCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (tmPaused || type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* p = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    if (len < 24) return;

    uint8_t subtype = p[0] & 0xFC;
    uint32_t bssidHash = 0;
    uint32_t ssidHash = 0;

    if (subtype == 0x80 && len >= 38) {
        bssidHash = hashBytes(p + 16, 6);
        ssidHash = beaconSsidHash(p, len);
    }

    portENTER_CRITICAL_ISR(&tmCounterMux);
    tmMgmtSec++;
    if (subtype == 0x80) {
        tmBeaconSec++;
        tmBeaconTotal++;
        if (bssidHash && rememberHash(tmBeaconBssidHashes, tmBeaconBssidHashCount, bssidHash)) {
            tmBeaconUniqueBssidSec++;
        }
        if (ssidHash && rememberHash(tmBeaconSsidHashes, tmBeaconSsidHashCount, ssidHash)) {
            tmBeaconUniqueSsidSec++;
        }
    } else if (subtype == 0x40) {
        tmProbeSec++;
        tmProbeTotal++;
    } else if (subtype == 0xC0) {
        tmDeauthSec++;
        tmDeauthTotal++;
    } else if (subtype == 0xA0) {
        tmDisassocSec++;
        tmDisassocTotal++;
    }
    portEXIT_CRITICAL_ISR(&tmCounterMux);
}

static void drawMeter(int score) {
    int x = 270, y = 48, w = 32, h = 126;
    tft.drawRect(x, y, w, h, UI_ACCENT);
    tft.fillRect(x + 2, y + 2, w - 4, h - 4, TFT_BLACK);
    int fillH = ((h - 4) * score) / 100;
    uint16_t col = riskColor(score);
    tft.fillRect(x + 2, y + h - 2 - fillH, w - 4, fillH, col);
}

static void drawThreatScreen() {
    int score = riskScore();
    uint16_t col = riskColor(score);

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(8, 7, "THREAT MON", TFT_WHITE, 1);
    drawStringCustom(194, 12, tmPaused ? "PAUSED" : "LIVE", tmPaused ? TFT_YELLOW : TFT_GREEN, 1);
    drawStringCustom(250, 12, "CH" + String(tmChannel), TFT_CYAN, 1);
    tft.drawFastHLine(0, 34, 320, UI_ACCENT);

    drawStringBig(12, 44, riskLabel(score), col, 2);
    drawStringCustom(14, 82, "SCORE:" + String(score) +
        " BASE:" + String(baselineRiskScore()) +
        " LIVE:" + String(tmLiveRisk), col, 1);
    drawMeter(score);

    drawStringCustom(14, 106, "APs:" + String(tmApCount) +
        " OPEN:" + String(tmOpenCount) +
        " WEAK:" + String(tmWeakCount), TFT_WHITE, 1);
    drawStringCustom(14, 124, "SSID DUP:" + String(tmDuplicateSsidCount) +
        "  BEAC/s:" + String(tmLastBeacon), UI_ACCENT, 1);

    uint16_t eventCol = (tmLastDeauth || tmLastDisassoc) ? TFT_RED : TFT_GREEN;
    drawStringCustom(14, 150, "DEAUTH/s:" + String(tmLastDeauth) +
        "  DISASSOC/s:" + String(tmLastDisassoc), eventCol, 1);
    drawStringCustom(14, 168, "BSSID/s:" + String(tmLastBeaconUniqueBssid) +
        " SSID/s:" + String(tmLastBeaconUniqueSsid), TFT_WHITE, 1);
    drawStringCustom(14, 186, "MGMT/s:" + String(tmLastMgmt) +
        " PROBE/s:" + String(tmLastProbe), TFT_WHITE, 1);

    String hint = tmPaused ? "OK:LIVE  UP/DN:CH  BACK/OK(H):EXIT"
                           : "OK:SAVE  UP/DN:CH  BACK/OK(H):EXIT";
    tft.drawFastHLine(0, 214, 320, UI_ACCENT);
    drawStringCustom(8, 222, hint, UI_ACCENT, 1);
}

static bool exportThreatReport() {
    String out;
    out.reserve(768 + tmApCount * 96);
    out += "CYBERDECK THREAT MONITOR\r\n";
    out += "Passive defensive report\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += "RiskScore: " + String(riskScore()) + "\r\n";
    out += "RiskLabel: " + String(riskLabel(riskScore())) + "\r\n";
    out += "BaseRisk: " + String(baselineRiskScore()) + "\r\n";
    out += "LiveRisk: " + String(tmLiveRisk) + "\r\n";
    out += "\r\nSummary\r\n";
    out += "APs: " + String(tmApCount) + "\r\n";
    out += "Open: " + String(tmOpenCount) + "\r\n";
    out += "Weak: " + String(tmWeakCount) + "\r\n";
    out += "DuplicateSSID: " + String(tmDuplicateSsidCount) + "\r\n";
    out += "DeauthTotal: " + String((unsigned long)tmDeauthTotal) + "\r\n";
    out += "DisassocTotal: " + String((unsigned long)tmDisassocTotal) + "\r\n";
    out += "BeaconTotal: " + String((unsigned long)tmBeaconTotal) + "\r\n";
    out += "ProbeTotal: " + String((unsigned long)tmProbeTotal) + "\r\n";
    out += "BeaconUniqueBssidLastSec: " + String((unsigned long)tmLastBeaconUniqueBssid) + "\r\n";
    out += "BeaconUniqueSsidLastSec: " + String((unsigned long)tmLastBeaconUniqueSsid) + "\r\n";
    out += "\r\nSSID,AUTH,CH,RSSI,BSSID_MASKED,FLAG\r\n";

    for (int i = 0; i < tmApCount; i++) {
        String flag = "OK";
        if (tmAps[i].auth == WIFI_AUTH_OPEN) flag = "OPEN";
        else if (isWeakAuth(tmAps[i].auth)) flag = "WEAK";

        out += safeField(String(tmAps[i].ssid)) + ",";
        out += String(authName(tmAps[i].auth)) + ",";
        out += String(tmAps[i].channel) + ",";
        out += String(tmAps[i].rssi) + ",";
        out += maskedBssid(tmAps[i].bssid) + ",";
        out += flag + "\r\n";
    }

    return sdWriteTextFile(TM_REPORT_PATH, out);
}

static void showExportResult(bool ok) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, ok ? TFT_GREEN : TFT_RED);
    drawStringBig(10, 8, ok ? "REPORT SAVED" : "SAVE ERROR", ok ? TFT_GREEN : TFT_RED, 1);
    tft.drawFastHLine(0, 34, 320, ok ? TFT_GREEN : TFT_RED);
    drawStringFit(20, 98, ok ? String(TM_REPORT_PATH) : "No se pudo escribir SD",
                  ok ? TFT_CYAN : TFT_YELLOW, 280, 2);
    tft.drawFastHLine(0, 214, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK/BACK: RETURN", UI_ACCENT, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
}

void runThreatMonitor() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);

    scanBaseline();

    tmMgmtSec = tmBeaconSec = tmProbeSec = 0;
    tmDeauthSec = tmDisassocSec = 0;
    tmBeaconTotal = tmProbeTotal = 0;
    tmDeauthTotal = tmDisassocTotal = 0;
    tmLastMgmt = tmLastBeacon = tmLastProbe = 0;
    tmLastDeauth = tmLastDisassoc = 0;
    tmLastBeaconUniqueBssid = 0;
    tmLastBeaconUniqueSsid = 0;
    tmLiveRisk = 0;
    tmBeaconUniqueBssidSec = 0;
    tmBeaconUniqueSsidSec = 0;
    tmBeaconBssidHashCount = 0;
    tmBeaconSsidHashCount = 0;
    memset(tmBeaconBssidHashes, 0, sizeof(tmBeaconBssidHashes));
    memset(tmBeaconSsidHashes, 0, sizeof(tmBeaconSsidHashes));
    tmAutoChannelIndex = 0;
    tmChannel = TM_AUTO_CHANNELS[tmAutoChannelIndex];
    tmPaused = false;

    WiFi.mode(WIFI_MODE_NULL);
    delay(80);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    wifi_promiscuous_filter_t filter;
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&threatCallback);
    esp_wifi_set_channel(tmChannel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);

    beep(1600, 35);
    delay(20);
    beep(2400, 45);

    unsigned long lastDraw = 0;
    unsigned long lastHop = millis();
    unsigned long lastSecond = millis();
    bool exitMonitor = false;
    drawThreatScreen();

    while (!exitMonitor) {
        unsigned long now = millis();

        if (!tmPaused && now - lastHop > TM_CHANNEL_MS) {
            tmAutoChannelIndex = (tmAutoChannelIndex + 1) % TM_AUTO_CHANNEL_COUNT;
            tmChannel = TM_AUTO_CHANNELS[tmAutoChannelIndex];
            esp_wifi_set_channel(tmChannel, WIFI_SECOND_CHAN_NONE);
            lastHop = now;
        }

        if (now - lastSecond > 1000) {
            portENTER_CRITICAL(&tmCounterMux);
            tmLastMgmt = tmMgmtSec;
            tmLastBeacon = tmBeaconSec;
            tmLastProbe = tmProbeSec;
            tmLastDeauth = tmDeauthSec;
            tmLastDisassoc = tmDisassocSec;
            tmLastBeaconUniqueBssid = tmBeaconUniqueBssidSec;
            tmLastBeaconUniqueSsid = tmBeaconUniqueSsidSec;
            tmMgmtSec = tmBeaconSec = tmProbeSec = 0;
            tmDeauthSec = tmDisassocSec = 0;
            tmBeaconUniqueBssidSec = 0;
            tmBeaconUniqueSsidSec = 0;
            tmBeaconBssidHashCount = 0;
            tmBeaconSsidHashCount = 0;
            portEXIT_CRITICAL(&tmCounterMux);
            updateLiveRisk();
            lastSecond = now;
        }

        if (now - lastDraw > TM_DRAW_MS) {
            drawThreatScreen();
            lastDraw = now;
        }

        NavAction action = readNavAction(130);
        if (action == NAV_BACK) {
            exitMonitor = true;
        } else if (action == NAV_UP || action == NAV_DOWN) {
            tmPaused = true;
            if (action == NAV_UP) tmChannel = (tmChannel % 13) + 1;
            else tmChannel = (tmChannel == 1) ? 13 : tmChannel - 1;
            esp_wifi_set_channel(tmChannel, WIFI_SECOND_CHAN_NONE);
            beep(2000, 18);
            drawThreatScreen();
        } else if (action == NAV_ENTER) {
            unsigned long pressedAt = millis();
            while (isEnterPressed() && millis() - pressedAt < 650) delay(5);
            if (millis() - pressedAt >= 650) {
                exitMonitor = true;
            } else if (tmPaused) {
                tmPaused = false;
                lastHop = millis();
                beep(2600, 35);
                drawThreatScreen();
            } else {
                bool ok = exportThreatReport();
                beep(ok ? 2400 : 900, 45);
                showExportResult(ok);
                drawThreatScreen();
            }
        }

        delay(8);
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_stop();
    esp_wifi_deinit();
    WiFi.mode(WIFI_OFF);
    beep(1200, 45);
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);
}
