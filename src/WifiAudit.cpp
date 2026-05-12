#include "WifiAudit.h"

#include <Arduino.h>
#include <WiFi.h>

#include "DisplayTFT.h"
#include "Input.h"
#include "PepeDraw.h"
#include "PeripheralTools.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

#define WA_MAX_APS       48
#define WA_MAX_FINDINGS  34
#define WA_REPORT_PATH   "/WIFI_DEFENSE.txt"

struct WaAp {
    char ssid[33];
    char bssid[18];
    uint8_t channel;
    int8_t rssi;
    uint8_t auth;
    bool hidden;
};

struct WaFinding {
    String title;
    String detail;
    uint16_t color;
    int severity;
};

static WaAp waAps[WA_MAX_APS];
static WaFinding waFindings[WA_MAX_FINDINGS];
static int waApCount = 0;
static int waFindingCount = 0;
static int waOpenCount = 0;
static int waWeakCount = 0;
static int waHiddenCount = 0;
static int waDuplicateGroups = 0;
static int waSuspiciousGroups = 0;
static int waRiskScore = 0;

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

static bool weakAuth(uint8_t auth) {
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

static void addFinding(const String& title, const String& detail,
                       uint16_t color, int severity) {
    if (waFindingCount >= WA_MAX_FINDINGS) return;
    waFindings[waFindingCount++] = { title, detail, color, severity };
}

static void sortFindings() {
    for (int i = 0; i < waFindingCount - 1; i++) {
        for (int j = i + 1; j < waFindingCount; j++) {
            if (waFindings[j].severity > waFindings[i].severity) {
                WaFinding tmp = waFindings[i];
                waFindings[i] = waFindings[j];
                waFindings[j] = tmp;
            }
        }
    }
}

static const char* riskLabel() {
    if (waRiskScore < 20) return "LOW";
    if (waRiskScore < 45) return "WATCH";
    if (waRiskScore < 70) return "RISK";
    return "ALERT";
}

static uint16_t riskColor() {
    if (waRiskScore < 20) return TFT_GREEN;
    if (waRiskScore < 45) return TFT_CYAN;
    if (waRiskScore < 70) return TFT_YELLOW;
    return TFT_RED;
}

static void drawFrame(const char* title) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(10, 8, title, TFT_WHITE, 1);
    tft.drawFastHLine(0, 34, 320, UI_ACCENT);
    tft.drawFastHLine(0, 214, 320, UI_ACCENT);
}

static void drawScanScreen(const char* msg) {
    drawFrame("WIFI AUDIT");
    drawStringCustom(28, 94, msg, TFT_CYAN, 2);
    drawStringCustom(28, 126, "Modo defensivo / solo lectura", UI_ACCENT, 1);
}

static void scanWifi() {
    memset(waAps, 0, sizeof(waAps));
    waApCount = 0;
    waOpenCount = 0;
    waWeakCount = 0;
    waHiddenCount = 0;
    waDuplicateGroups = 0;
    waSuspiciousGroups = 0;
    waFindingCount = 0;
    waRiskScore = 0;

    drawScanScreen("Escaneando redes...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(120);

    int n = WiFi.scanNetworks(false, true);
    if (n < 0) n = 0;
    if (n > WA_MAX_APS) n = WA_MAX_APS;

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        waAps[i].hidden = ssid.length() == 0;
        if (waAps[i].hidden) ssid = "<hidden>";
        ssid.toCharArray(waAps[i].ssid, sizeof(waAps[i].ssid));
        WiFi.BSSIDstr(i).toCharArray(waAps[i].bssid, sizeof(waAps[i].bssid));
        waAps[i].channel = (uint8_t)WiFi.channel(i);
        waAps[i].rssi = (int8_t)WiFi.RSSI(i);
        waAps[i].auth = WiFi.encryptionType(i);
        waApCount++;

        if (waAps[i].hidden) waHiddenCount++;
        if (waAps[i].auth == WIFI_AUTH_OPEN) waOpenCount++;
        if (weakAuth(waAps[i].auth)) waWeakCount++;
    }

    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
}

static void analyzeWifi() {
    for (int i = 0; i < waApCount; i++) {
        if (waAps[i].auth == WIFI_AUTH_OPEN) {
            addFinding("OPEN NETWORK",
                       String(waAps[i].ssid) + " CH" + String(waAps[i].channel) +
                       " RSSI " + String(waAps[i].rssi),
                       TFT_RED, 85);
        } else if (weakAuth(waAps[i].auth)) {
            addFinding("WEAK SECURITY",
                       String(waAps[i].ssid) + " uses " + String(authName(waAps[i].auth)),
                       TFT_YELLOW, 70);
        }
    }

    for (int i = 0; i < waApCount; i++) {
        if (waAps[i].hidden) continue;

        bool seen = false;
        for (int k = 0; k < i; k++) {
            if (!waAps[k].hidden && strcmp(waAps[k].ssid, waAps[i].ssid) == 0) {
                seen = true;
                break;
            }
        }
        if (seen) continue;

        int count = 0;
        int authMask = 0;
        int minCh = 99;
        int maxCh = 0;
        bool hasOpen = false;
        bool hasSecure = false;

        for (int j = i; j < waApCount; j++) {
            if (waAps[j].hidden || strcmp(waAps[j].ssid, waAps[i].ssid) != 0) continue;
            count++;
            authMask |= (1 << min((int)waAps[j].auth, 7));
            if (waAps[j].channel < minCh) minCh = waAps[j].channel;
            if (waAps[j].channel > maxCh) maxCh = waAps[j].channel;
            if (waAps[j].auth == WIFI_AUTH_OPEN) hasOpen = true;
            else hasSecure = true;
        }

        if (count <= 1) continue;
        waDuplicateGroups++;

        bool authMismatch = (authMask & (authMask - 1)) != 0;
        bool channelSpread = (maxCh - minCh) > 5;
        bool suspicious = authMismatch || (hasOpen && hasSecure) || channelSpread || count >= 5;

        if (suspicious) {
            waSuspiciousGroups++;
            addFinding("POSSIBLE CLONE",
                       String(waAps[i].ssid) + " APs:" + String(count) +
                       " CH" + String(minCh) + "-" + String(maxCh),
                       TFT_RED, 95);
        } else {
            addFinding("MULTI-BSSID",
                       String(waAps[i].ssid) + " APs:" + String(count),
                       TFT_CYAN, 45);
        }
    }

    if (waHiddenCount > 0) {
        addFinding("HIDDEN SSIDS",
                   String(waHiddenCount) + " redes sin nombre visible",
                   TFT_YELLOW, 45);
    }

    waRiskScore = waOpenCount * 7 + waWeakCount * 9 +
                  waSuspiciousGroups * 24 + waDuplicateGroups * 5 +
                  waHiddenCount * 2;
    if (waRiskScore > 100) waRiskScore = 100;

    if (waFindingCount == 0) {
        addFinding("NO CRITICAL FINDINGS",
                   "No se detectaron redes abiertas o clones obvios.",
                   TFT_GREEN, 10);
    }

    sortFindings();
}

static void drawAuditScreen(int scroll) {
    drawFrame("WIFI AUDIT");
    uint16_t rc = riskColor();
    drawStringBig(12, 42, riskLabel(), rc, 2);
    drawStringCustom(154, 44, "SCORE " + String(waRiskScore) + "/100", rc, 1);
    drawStringCustom(154, 60, "AP:" + String(waApCount) +
        " OPEN:" + String(waOpenCount) +
        " WEAK:" + String(waWeakCount), TFT_WHITE, 1);
    drawStringCustom(154, 76, "CLONE:" + String(waSuspiciousGroups) +
        " DUP:" + String(waDuplicateGroups), UI_ACCENT, 1);

    const int listY = 100;
    const int rowH = 27;
    const int visible = 4;
    for (int row = 0; row < visible; row++) {
        int idx = scroll + row;
        int y = listY + row * rowH;
        tft.fillRect(8, y - 2, 304, rowH - 2, TFT_BLACK);
        if (idx >= waFindingCount) continue;
        tft.drawRect(8, y - 2, 304, rowH - 2, waFindings[idx].color);
        drawStringFit(14, y + 2, waFindings[idx].title,
                      waFindings[idx].color, 130, 1);
        drawStringFit(118, y + 2, waFindings[idx].detail,
                      TFT_WHITE, 188, 1);
    }

    if (waFindingCount > visible) {
        int trackH = visible * rowH;
        int barH = (visible * trackH) / waFindingCount;
        if (barH < 8) barH = 8;
        int barY = listY + (scroll * (trackH - barH)) / (waFindingCount - visible);
        tft.fillRect(315, listY, 3, trackH, TFT_BLACK);
        tft.fillRect(315, barY, 3, barH, TFT_CYAN);
    }

    drawStringCustom(8, 222, "UP/DN:MOVE  OK:SAVE SD  BACK:EXIT", UI_ACCENT, 1);
}

static bool exportAudit() {
    String out;
    out.reserve(1024 + waApCount * 88 + waFindingCount * 80);
    out += "CYBERDECK WIFI DEFENSE AUDIT\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += "RiskScore: " + String(waRiskScore) + "\r\n";
    out += "RiskLabel: " + String(riskLabel()) + "\r\n";
    out += "APs: " + String(waApCount) + "\r\n";
    out += "Open: " + String(waOpenCount) + "\r\n";
    out += "Weak: " + String(waWeakCount) + "\r\n";
    out += "Hidden: " + String(waHiddenCount) + "\r\n";
    out += "DuplicateSSIDGroups: " + String(waDuplicateGroups) + "\r\n";
    out += "SuspiciousCloneGroups: " + String(waSuspiciousGroups) + "\r\n";

    out += "\r\nFindings\r\n";
    for (int i = 0; i < waFindingCount; i++) {
        out += String(i + 1) + ". " + waFindings[i].title +
               " - " + waFindings[i].detail + "\r\n";
    }

    out += "\r\nSSID,AUTH,CH,RSSI,BSSID_MASKED\r\n";
    for (int i = 0; i < waApCount; i++) {
        out += safeField(String(waAps[i].ssid)) + ",";
        out += String(authName(waAps[i].auth)) + ",";
        out += String(waAps[i].channel) + ",";
        out += String(waAps[i].rssi) + ",";
        out += maskedBssid(waAps[i].bssid) + "\r\n";
    }

    return sdWriteTextFile(WA_REPORT_PATH, out);
}

static void showSaveResult(bool ok) {
    drawFrame(ok ? "SAVE OK" : "SAVE ERROR");
    drawStringFit(20, 98, ok ? String(WA_REPORT_PATH) : "No se pudo escribir SD",
                  ok ? TFT_CYAN : TFT_YELLOW, 280, 2);
    drawStringCustom(10, 222, "OK/BACK: RETURN", UI_ACCENT, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);
}

void runWifiAudit() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);

    scanWifi();
    analyzeWifi();

    int scroll = 0;
    drawAuditScreen(scroll);
    beep(1800, 35);

    bool exitAudit = false;
    while (!exitAudit) {
        NavAction action = readNavAction(120);
        if (action == NAV_BACK) {
            exitAudit = true;
        } else if (action == NAV_UP) {
            if (scroll > 0) scroll--;
            else scroll = max(0, waFindingCount - 4);
            drawAuditScreen(scroll);
            beep(2200, 15);
        } else if (action == NAV_DOWN) {
            int maxScroll = max(0, waFindingCount - 4);
            if (scroll < maxScroll) scroll++;
            else scroll = 0;
            drawAuditScreen(scroll);
            beep(2200, 15);
        } else if (action == NAV_ENTER) {
            bool ok = exportAudit();
            beep(ok ? 2400 : 900, 45);
            showSaveResult(ok);
            drawAuditScreen(scroll);
        }
        delay(8);
    }

    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);
}
