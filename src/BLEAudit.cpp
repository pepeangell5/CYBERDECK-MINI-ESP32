#include "BLEAudit.h"

#include <Arduino.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>

#include "DisplayTFT.h"
#include "Input.h"
#include "PepeDraw.h"
#include "PeripheralTools.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

#define BLEA_MAX_DEVICES 42
#define BLEA_VISIBLE_ROWS 5
#define BLEA_SCAN_TIME_S 2
#define BLEA_STALE_MS 45000UL
#define BLEA_REPORT_PATH "/BLE_AUDIT.txt"

struct BleAuditDevice {
    char name[25];
    char mac[18];
    int rssi;
    int minRssi;
    int maxRssi;
    uint8_t addrType;
    uint8_t serviceCount;
    bool hasName;
    bool hasMfg;
    uint16_t vendorId;
    uint16_t hits;
    uint32_t firstSeen;
    uint32_t lastSeen;
};

static BleAuditDevice bleDevices[BLEA_MAX_DEVICES];
static int bleDeviceCount = 0;
static int bleNamedCount = 0;
static int blePublicCount = 0;
static int bleRandomCount = 0;
static int blePrivateCount = 0;
static int bleCloseCount = 0;
static int bleMfgCount = 0;
static int bleExposureScore = 0;
static unsigned long lastNewBeepMs = 0;

static const char* bleVendorName(uint16_t id) {
    switch (id) {
        case 0x004C: return "Apple";
        case 0x0075: return "Samsung";
        case 0x0006: return "Microsoft";
        case 0x00E0: return "Google";
        case 0x038F: return "Xiaomi";
        case 0x02E1: return "Amazon";
        case 0x0157: return "Fitbit";
        case 0x0059: return "Nordic";
        case 0x0499: return "Ruuvi";
        case 0x0131: return "Huawei";
        case 0x0087: return "Garmin";
        case 0x012D: return "Sony";
        case 0x0505: return "Bose";
        case 0x01DA: return "Logitech";
        case 0x0154: return "Withings";
        case 0x022B: return "Tile";
        case 0x0397: return "Espressif";
        default:     return nullptr;
    }
}

static uint8_t firstMacByte(const char* mac) {
    if (!mac || strlen(mac) < 2) return 0;
    char buf[3] = { mac[0], mac[1], '\0' };
    return (uint8_t)strtoul(buf, nullptr, 16);
}

static bool isRandomAddress(uint8_t addrType) {
    return addrType == 1 || addrType == 3;
}

static const char* addrTypeLabel(uint8_t t) {
    switch (t) {
        case 0: return "Public";
        case 1: return "Random";
        case 2: return "Public-ID";
        case 3: return "Random-ID";
        default: return "Unknown";
    }
}

static const char* privacyLabel(const BleAuditDevice& d) {
    if (!isRandomAddress(d.addrType)) return "TRACKABLE";
    uint8_t top = firstMacByte(d.mac) & 0xC0;
    if (top == 0x40) return "RPA";
    if (top == 0x00) return "NRPA";
    if (top == 0xC0) return "STATIC";
    return "RANDOM";
}

static bool isPrivateRandom(const BleAuditDevice& d) {
    if (!isRandomAddress(d.addrType)) return false;
    uint8_t top = firstMacByte(d.mac) & 0xC0;
    return top == 0x40 || top == 0x00;
}

static bool isTrackableAddress(const BleAuditDevice& d) {
    if (!isRandomAddress(d.addrType)) return true;
    uint8_t top = firstMacByte(d.mac) & 0xC0;
    return top == 0xC0;
}

static const char* rssiLabel(int rssi) {
    if (rssi >= -50) return "VERY CLOSE";
    if (rssi >= -65) return "CLOSE";
    if (rssi >= -80) return "NEAR";
    return "FAR";
}

static int rssiBars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -85) return 2;
    if (rssi >= -95) return 1;
    return 0;
}

static String maskedMac(const char* mac) {
    String m(mac);
    if (m.length() < 17) return m;
    return m.substring(0, 8) + ":xx:xx:xx";
}

static String safeField(const String& in) {
    String out = in;
    out.replace("\r", " ");
    out.replace("\n", " ");
    out.replace(",", " ");
    return out;
}

static int findDevice(const char* mac) {
    for (int i = 0; i < bleDeviceCount; i++) {
        if (strcmp(bleDevices[i].mac, mac) == 0) return i;
    }
    return -1;
}

static void copyName(char* dst, size_t dstLen, const String& name) {
    if (dstLen == 0) return;
    String clean = name;
    clean.replace("\r", " ");
    clean.replace("\n", " ");
    clean.trim();
    clean.toCharArray(dst, dstLen);
}

static void upsertDevice(BLEAdvertisedDevice& ad) {
    String macStr = String(ad.getAddress().toString().c_str());
    char mac[18];
    macStr.toCharArray(mac, sizeof(mac));

    int idx = findDevice(mac);
    bool isNew = idx < 0;
    if (isNew) {
        if (bleDeviceCount >= BLEA_MAX_DEVICES) return;
        idx = bleDeviceCount++;
        memset(&bleDevices[idx], 0, sizeof(BleAuditDevice));
        strlcpy(bleDevices[idx].mac, mac, sizeof(bleDevices[idx].mac));
        bleDevices[idx].minRssi = 127;
        bleDevices[idx].maxRssi = -127;
        bleDevices[idx].firstSeen = millis();
    }

    BleAuditDevice& d = bleDevices[idx];
    d.rssi = ad.getRSSI();
    if (d.rssi < d.minRssi) d.minRssi = d.rssi;
    if (d.rssi > d.maxRssi) d.maxRssi = d.rssi;
    d.addrType = (uint8_t)ad.getAddressType();
    d.lastSeen = millis();
    if (d.hits < 65535) d.hits++;

    if (ad.haveName()) {
        String name = String(ad.getName().c_str());
        if (name.length() > 0) {
            copyName(d.name, sizeof(d.name), name);
            d.hasName = strlen(d.name) > 0;
        }
    }

    d.serviceCount = (uint8_t)min(ad.getServiceUUIDCount(), 255);
    d.hasMfg = ad.haveManufacturerData();
    if (d.hasMfg) {
        std::string md = ad.getManufacturerData();
        if (md.size() >= 2) {
            d.vendorId = ((uint16_t)(uint8_t)md[1] << 8) | (uint8_t)md[0];
        }
    }

    if (isNew && millis() - lastNewBeepMs > 180) {
        beep(2200, 12);
        lastNewBeepMs = millis();
    }
}

class BleAuditCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice ad) override {
        upsertDevice(ad);
    }
};

static BleAuditCallbacks bleCallbacks;

static void sortDevicesByRssi() {
    for (int i = 0; i < bleDeviceCount - 1; i++) {
        for (int j = i + 1; j < bleDeviceCount; j++) {
            if (bleDevices[j].rssi > bleDevices[i].rssi) {
                BleAuditDevice tmp = bleDevices[i];
                bleDevices[i] = bleDevices[j];
                bleDevices[j] = tmp;
            }
        }
    }
}

static void pruneStaleDevices() {
    uint32_t now = millis();
    for (int i = 0; i < bleDeviceCount; ) {
        if (now - bleDevices[i].lastSeen <= BLEA_STALE_MS) {
            i++;
            continue;
        }
        for (int j = i; j < bleDeviceCount - 1; j++) {
            bleDevices[j] = bleDevices[j + 1];
        }
        bleDeviceCount--;
    }
}

static void recomputeSummary() {
    bleNamedCount = 0;
    blePublicCount = 0;
    bleRandomCount = 0;
    blePrivateCount = 0;
    bleCloseCount = 0;
    bleMfgCount = 0;

    for (int i = 0; i < bleDeviceCount; i++) {
        const BleAuditDevice& d = bleDevices[i];
        if (d.hasName) bleNamedCount++;
        if (isRandomAddress(d.addrType)) bleRandomCount++;
        else blePublicCount++;
        if (isPrivateRandom(d)) blePrivateCount++;
        if (d.rssi >= -60) bleCloseCount++;
        if (d.hasMfg) bleMfgCount++;
    }

    int score = 0;
    score += blePublicCount * 7;
    score += (bleRandomCount - blePrivateCount) * 5;
    score += bleNamedCount * 5;
    score += bleCloseCount * 8;
    score += bleMfgCount * 2;
    if (bleDeviceCount > 12) score += 10;
    if (bleDeviceCount > 24) score += 10;
    if (score > 100) score = 100;
    bleExposureScore = score;
}

static const char* exposureLabel() {
    if (bleExposureScore < 20) return "LOW";
    if (bleExposureScore < 45) return "WATCH";
    if (bleExposureScore < 70) return "RISK";
    return "ALERT";
}

static uint16_t exposureColor() {
    if (bleExposureScore < 20) return TFT_GREEN;
    if (bleExposureScore < 45) return TFT_CYAN;
    if (bleExposureScore < 70) return TFT_YELLOW;
    return TFT_RED;
}

static void drawFrame(const char* title) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(10, 8, title, TFT_WHITE, 1);
    tft.drawFastHLine(0, 34, 320, TFT_WHITE);
    tft.drawFastHLine(0, 214, 320, TFT_WHITE);
}

static void drawBars(int x, int y, int rssi, bool selected) {
    int bars = rssiBars(rssi);
    for (int b = 0; b < 4; b++) {
        int bh = 4 + b * 3;
        uint16_t c = selected ? TFT_BLACK :
            (b < bars ? (bars >= 3 ? TFT_GREEN : bars >= 2 ? TFT_YELLOW : TFT_ORANGE)
                      : TFT_DARKGREY);
        int bx = x + b * 6;
        int by = y + 15 - bh;
        if (b < bars) tft.fillRect(bx, by, 4, bh, c);
        else tft.drawRect(bx, by, 4, bh, c);
    }
}

static void drawAuditList(int cursor, int scroll) {
    uint16_t scoreCol = exposureColor();
    tft.fillRect(1, 35, 318, 178, TFT_BLACK);

    drawStringBig(10, 42, exposureLabel(), scoreCol, 2);
    drawStringCustom(150, 42, "SCORE " + String(bleExposureScore) + "/100", scoreCol, 1);
    drawStringCustom(150, 58, "DEV:" + String(bleDeviceCount) +
        " NAMED:" + String(bleNamedCount), TFT_WHITE, 1);
    drawStringCustom(150, 74, "PUB:" + String(blePublicCount) +
        " RAND:" + String(bleRandomCount) +
        " PRIV:" + String(blePrivateCount), UI_ACCENT, 1);

    const int listY = 94;
    const int rowH = 23;

    if (bleDeviceCount == 0) {
        drawStringCustom(52, 126, "Searching BLE advertisements...", TFT_CYAN, 1);
        drawStringCustom(52, 144, "Passive defensive scan", UI_ACCENT, 1);
    }

    for (int row = 0; row < BLEA_VISIBLE_ROWS; row++) {
        int idx = scroll + row;
        int y = listY + row * rowH;
        tft.fillRect(8, y - 2, 304, rowH - 2, TFT_BLACK);
        if (idx >= bleDeviceCount) continue;

        const BleAuditDevice& d = bleDevices[idx];
        bool selected = idx == cursor;
        uint16_t bg = selected ? TFT_WHITE : TFT_BLACK;
        uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
        uint16_t sub = selected ? TFT_BLACK : UI_ACCENT;
        uint16_t privacyCol = selected ? TFT_BLACK :
            (isTrackableAddress(d) ? TFT_YELLOW : TFT_CYAN);

        tft.fillRect(8, y - 2, 304, rowH - 2, bg);
        tft.drawRect(8, y - 2, 304, rowH - 2, TFT_WHITE);

        String title;
        if (d.hasName) title = String(d.name);
        else if (d.hasMfg && bleVendorName(d.vendorId)) title = String(bleVendorName(d.vendorId));
        else title = "<unnamed>";
        drawStringFit(14, y + 1, title, fg, 128, 1);

        drawStringFit(14, y + 13, maskedMac(d.mac), sub, 102, 1);
        drawStringCustom(130, y + 13, privacyLabel(d), privacyCol, 1);
        drawStringCustom(218, y + 2, String(d.rssi) + "dBm", fg, 1);
        drawBars(282, y + 2, d.rssi, selected);
    }

    if (bleDeviceCount > BLEA_VISIBLE_ROWS) {
        int trackH = BLEA_VISIBLE_ROWS * rowH;
        int barH = (BLEA_VISIBLE_ROWS * trackH) / bleDeviceCount;
        if (barH < 8) barH = 8;
        int barY = listY + (scroll * (trackH - barH)) / (bleDeviceCount - BLEA_VISIBLE_ROWS);
        tft.fillRect(315, listY, 3, trackH, TFT_BLACK);
        tft.fillRect(315, barY, 3, barH, TFT_CYAN);
    }

    drawStringCustom(8, 222, "UP/DN  OK:INFO  OK-H:SAVE  BACK:EXIT", UI_ACCENT, 1);
}

static bool exportBleAudit() {
    recomputeSummary();

    String out;
    out.reserve(1200 + bleDeviceCount * 110);
    out += "CYBERDECK BLE DEFENSE AUDIT\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += "ExposureScore: " + String(bleExposureScore) + "\r\n";
    out += "ExposureLabel: " + String(exposureLabel()) + "\r\n";
    out += "Devices: " + String(bleDeviceCount) + "\r\n";
    out += "Named: " + String(bleNamedCount) + "\r\n";
    out += "PublicAddr: " + String(blePublicCount) + "\r\n";
    out += "RandomAddr: " + String(bleRandomCount) + "\r\n";
    out += "PrivateRandom: " + String(blePrivateCount) + "\r\n";
    out += "CloseDevices: " + String(bleCloseCount) + "\r\n";
    out += "ManufacturerData: " + String(bleMfgCount) + "\r\n";
    out += "\r\nNotes\r\n";
    out += "TRACKABLE means public or static-random address observed.\r\n";
    out += "RPA/NRPA are privacy-oriented random address types.\r\n";
    out += "Report masks MAC addresses by default.\r\n";

    out += "\r\nMAC_MASKED,ADDR_TYPE,PRIVACY,RSSI_MIN,RSSI_NOW,RSSI_MAX,HITS,VENDOR,NAME,SERVICES\r\n";
    for (int i = 0; i < bleDeviceCount; i++) {
        const BleAuditDevice& d = bleDevices[i];
        const char* vendor = (d.hasMfg && d.vendorId != 0) ? bleVendorName(d.vendorId) : nullptr;
        char vendorBuf[12];
        if (!vendor && d.hasMfg && d.vendorId != 0) {
            snprintf(vendorBuf, sizeof(vendorBuf), "0x%04X", d.vendorId);
            vendor = vendorBuf;
        }

        out += maskedMac(d.mac) + ",";
        out += String(addrTypeLabel(d.addrType)) + ",";
        out += String(privacyLabel(d)) + ",";
        out += String(d.minRssi) + ",";
        out += String(d.rssi) + ",";
        out += String(d.maxRssi) + ",";
        out += String(d.hits) + ",";
        out += safeField(vendor ? String(vendor) : String("--")) + ",";
        out += safeField(d.hasName ? String(d.name) : String("<unnamed>")) + ",";
        out += String(d.serviceCount) + "\r\n";
    }

    return sdWriteTextFile(BLEA_REPORT_PATH, out);
}

static void showSaveResult(bool ok) {
    drawFrame(ok ? "SAVE OK" : "SAVE ERROR");
    drawStringFit(20, 98, ok ? String(BLEA_REPORT_PATH) : "No se pudo escribir SD",
                  ok ? TFT_CYAN : TFT_YELLOW, 280, 2);
    drawStringCustom(10, 222, "OK/BACK: RETURN", UI_ACCENT, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);
}

static void drawDetails(const BleAuditDevice& d) {
    drawFrame("BLE DEVICE");

    int y = 42;
    drawStringFit(10, y, d.hasName ? String(d.name) : String("<unnamed>"),
                  TFT_WHITE, 300, 2);
    y += 24;
    drawStringCustom(10, y, "MAC: " + String(d.mac), UI_ACCENT, 1); y += 15;
    drawStringCustom(10, y, "RSSI: " + String(d.rssi) + " dBm  " +
        String(rssiLabel(d.rssi)), TFT_WHITE, 1); y += 15;
    drawStringCustom(10, y, "Range: " + String(d.minRssi) + " / " +
        String(d.maxRssi) + " dBm", UI_ACCENT, 1); y += 15;
    drawStringCustom(10, y, "Addr: " + String(addrTypeLabel(d.addrType)) +
        "  " + String(privacyLabel(d)), isTrackableAddress(d) ? TFT_YELLOW : TFT_CYAN, 1); y += 15;

    const char* vendor = (d.hasMfg && d.vendorId != 0) ? bleVendorName(d.vendorId) : nullptr;
    String vendorLine = "Vendor: ";
    if (vendor) {
        vendorLine += vendor;
    } else if (d.hasMfg && d.vendorId != 0) {
        char buf[12];
        snprintf(buf, sizeof(buf), "0x%04X", d.vendorId);
        vendorLine += buf;
    } else {
        vendorLine += "--";
    }
    drawStringCustom(10, y, vendorLine, TFT_WHITE, 1); y += 15;
    drawStringCustom(10, y, "Services: " + String(d.serviceCount) +
        "  Hits: " + String(d.hits), UI_ACCENT, 1); y += 20;

    tft.drawFastHLine(10, y, 300, TFT_WHITE); y += 9;
    if (isTrackableAddress(d)) {
        drawStringFit(10, y, "Privacy note: trackable address. Prefer random/private BLE mode if this is your device.",
                      TFT_YELLOW, 300, 1);
    } else {
        drawStringFit(10, y, "Privacy note: random/private address observed.",
                      TFT_CYAN, 300, 1);
    }

    drawStringCustom(8, 222, "OK/BACK:LIST  OK-H:SAVE REPORT", UI_ACCENT, 1);
}

void runBLEAudit() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);
    flushNavInput();

    memset(bleDevices, 0, sizeof(bleDevices));
    bleDeviceCount = 0;
    recomputeSummary();

    drawFrame("BLE DEFENSE");
    drawAuditList(0, 0);
    beep(1500, 35);
    delay(20);
    beep(2100, 45);

    BLEDevice::init("");
    BLEScan* scanner = BLEDevice::getScan();
    scanner->setAdvertisedDeviceCallbacks(&bleCallbacks, false);
    scanner->setActiveScan(false);
    scanner->setInterval(120);
    scanner->setWindow(90);
    scanner->start(BLEA_SCAN_TIME_S, nullptr, false);

    bool exitAudit = false;
    bool inDetails = false;
    int cursor = 0;
    int scroll = 0;
    int detailIdx = 0;
    unsigned long lastScanStart = millis();
    bool needsDraw = false;

    while (!exitAudit) {
        if (millis() - lastScanStart > BLEA_SCAN_TIME_S * 1000UL + 160) {
            scanner->clearResults();
            scanner->start(BLEA_SCAN_TIME_S, nullptr, false);
            lastScanStart = millis();
            pruneStaleDevices();
            sortDevicesByRssi();
            recomputeSummary();
            if (cursor >= bleDeviceCount) cursor = max(0, bleDeviceCount - 1);
            if (scroll > cursor) scroll = cursor;
            if (cursor >= scroll + BLEA_VISIBLE_ROWS) scroll = cursor - BLEA_VISIBLE_ROWS + 1;
            needsDraw = !inDetails;
        }

        if (!inDetails && needsDraw) {
            drawAuditList(cursor, scroll);
            needsDraw = false;
        }

        NavAction action = readNavAction(105);
        if (action == NAV_NONE && isBackPressed()) action = NAV_BACK;

        if (!inDetails) {
            if (action == NAV_BACK) {
                exitAudit = true;
            } else if (action == NAV_UP && bleDeviceCount > 0) {
                cursor = (cursor - 1 + bleDeviceCount) % bleDeviceCount;
                if (cursor < scroll) scroll = cursor;
                if (cursor >= scroll + BLEA_VISIBLE_ROWS) scroll = cursor - BLEA_VISIBLE_ROWS + 1;
                drawAuditList(cursor, scroll);
                beep(2200, 15);
                needsDraw = false;
            } else if (action == NAV_DOWN && bleDeviceCount > 0) {
                cursor = (cursor + 1) % bleDeviceCount;
                if (cursor < scroll) scroll = cursor;
                if (cursor >= scroll + BLEA_VISIBLE_ROWS) scroll = cursor - BLEA_VISIBLE_ROWS + 1;
                drawAuditList(cursor, scroll);
                beep(2200, 15);
                needsDraw = false;
            } else if (action == NAV_ENTER) {
                bool held = waitOkReleaseWasLong();
                if (held) {
                    bool ok = exportBleAudit();
                    beep(ok ? 2400 : 900, 45);
                    showSaveResult(ok);
                    drawFrame("BLE DEFENSE");
                    drawAuditList(cursor, scroll);
                    flushNavInput();
                } else if (bleDeviceCount > 0) {
                    detailIdx = cursor;
                    if (detailIdx >= bleDeviceCount) detailIdx = bleDeviceCount - 1;
                    inDetails = true;
                    drawDetails(bleDevices[detailIdx]);
                    beep(1800, 35);
                }
                needsDraw = false;
            }
        } else {
            if (action == NAV_BACK) {
                inDetails = false;
                drawFrame("BLE DEFENSE");
                drawAuditList(cursor, scroll);
                flushNavInput();
                needsDraw = false;
            } else if (action == NAV_ENTER) {
                bool held = waitOkReleaseWasLong();
                if (held) {
                    bool ok = exportBleAudit();
                    beep(ok ? 2400 : 900, 45);
                    showSaveResult(ok);
                    drawDetails(bleDevices[detailIdx]);
                    flushNavInput();
                } else {
                    inDetails = false;
                    drawFrame("BLE DEFENSE");
                    drawAuditList(cursor, scroll);
                }
                needsDraw = false;
            }
        }

        delay(8);
    }

    scanner->stop();
    scanner->clearResults();
    BLEDevice::deinit(false);
    tft.fillScreen(TFT_BLACK);

    beep(1600, 35);
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);
    flushNavInput();
}
