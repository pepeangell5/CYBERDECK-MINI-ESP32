#include "PeripheralTools.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <math.h>

#include "DisplayTFT.h"
#include "Input.h"
#include "MenuSystem.h"
#include "PepeDraw.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

static HardwareSerial gpsSerial(1);
static TinyGPSPlus gps;
static SPIClass sdSPI(HSPI);
static bool sdStarted = false;
static uint32_t sdMountHz = 0;

static void beginGpsPort() {
    gpsSerial.end();
    delay(20);
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void initPeripherals() {
    beginGpsPort();
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    pinMode(SD_MISO_PIN, INPUT_PULLUP);
    pinMode(VBAT_ADC_PIN, INPUT);
    analogSetPinAttenuation(VBAT_ADC_PIN, ADC_11db);
}

static void drainGps(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        while (gpsSerial.available()) gps.encode(gpsSerial.read());
        delay(2);
    }
}

static bool beginSd() {
    if (sdStarted) return true;

    pinMode(TFT_CS_PIN, OUTPUT);
    digitalWrite(TFT_CS_PIN, HIGH);
    pinMode(NRF1_CSN_PIN, OUTPUT);
    digitalWrite(NRF1_CSN_PIN, HIGH);
    pinMode(NRF2_CSN_PIN, OUTPUT);
    digitalWrite(NRF2_CSN_PIN, HIGH);
    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    const uint32_t speeds[] = { 4000000, 10000000, 20000000 };
    for (uint8_t i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
        SD.end();
        digitalWrite(SD_CS_PIN, HIGH);
        delay(80);
        if (SD.begin(SD_CS_PIN, sdSPI, speeds[i])) {
            sdStarted = true;
            sdMountHz = speeds[i];
            return true;
        }
    }

    sdMountHz = 0;
    return sdStarted;
}

bool sdWriteTextFile(const char* path, const String& content) {
    if (!beginSd()) return false;

    if (SD.exists(path)) SD.remove(path);
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;

    file.print(content);
    file.flush();
    file.close();

    File check = SD.open(path, FILE_READ);
    if (!check) return false;
    size_t size = check.size();
    check.close();
    return size > 0;
}

static bool sdAppendTextFile(const char* path, const String& content) {
    if (!beginSd()) return false;

    File file = SD.open(path, FILE_APPEND);
    if (!file) file = SD.open(path, FILE_WRITE);
    if (!file) return false;

    size_t written = file.print(content);
    file.flush();
    file.close();
    return written == content.length();
}

static bool sdFileHasData(const char* path) {
    if (!beginSd()) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    bool hasData = file.size() > 0;
    file.close();
    return hasData;
}

static void drawToolFrame(const char* title) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(10, 8, title, TFT_WHITE, 1);
    tft.drawFastHLine(0, 34, 320, TFT_WHITE);
    tft.drawFastHLine(0, 214, 320, TFT_WHITE);
    drawStringCustom(10, 222, "BACK/OK: RETURN", TFT_WHITE, 1);
}

static String gpsField(bool valid, const String& value) {
    return valid ? value : String("--");
}

static bool gpsFreshFix(uint32_t maxAgeMs = 5000) {
    return gps.location.isValid() && gps.location.age() < maxAgeMs;
}

static int gpsSatCount() {
    return gps.satellites.isValid() ? gps.satellites.value() : 0;
}

static String gpsUtcStamp() {
    if (!gps.date.isValid() || !gps.time.isValid()) return String("--");

    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    return String(buf);
}

static String gpsAgeText() {
    if (!gps.location.isValid()) return String("--");
    return String(gps.location.age()) + "ms";
}

static String gpsHdopText() {
    return gps.hdop.isValid() ? String(gps.hdop.hdop(), 1) : String("--");
}

static const char* gpsCardinal(double deg) {
    static const char* dirs[] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    int idx = (int)((deg + 11.25) / 22.5);
    idx %= 16;
    return dirs[idx];
}

static int gpsQualityScore() {
    int sats = gpsSatCount();
    int score = sats * 8;
    if (score > 70) score = 70;

    if (gps.hdop.isValid()) {
        float hdop = gps.hdop.hdop();
        if (hdop <= 1.2f) score += 30;
        else if (hdop <= 2.0f) score += 24;
        else if (hdop <= 4.0f) score += 14;
        else if (hdop <= 8.0f) score += 6;
    }

    if (gpsFreshFix()) score += 10;
    if (score > 100) score = 100;
    return score;
}

static uint16_t gpsQualityColor(int score) {
    if (score >= 70) return TFT_GREEN;
    if (score >= 40) return TFT_YELLOW;
    return TFT_ORANGE;
}

static void drawGpsQualityBar(int x, int y, int w, int h, int score) {
    uint16_t col = gpsQualityColor(score);
    tft.drawRect(x, y, w, h, TFT_WHITE);
    tft.fillRect(x + 2, y + 2, w - 4, h - 4, TFT_BLACK);
    int fillW = ((w - 4) * score) / 100;
    if (fillW > 0) tft.fillRect(x + 2, y + 2, fillW, h - 4, col);
}

static String gpsCsvHeader() {
    return "UTC,Millis,Lat,Lng,AltM,SpeedKmph,CourseDeg,Sat,HDOP,AgeMs,Event\r\n";
}

static bool ensureGpsCsv(const char* path) {
    if (sdFileHasData(path)) return true;
    return sdAppendTextFile(path, gpsCsvHeader());
}

static String gpsCsvRow(const char* eventName) {
    String row;
    row.reserve(128);
    row += gpsUtcStamp() + ",";
    row += String(millis()) + ",";
    row += gpsField(gps.location.isValid(), String(gps.location.lat(), 6)) + ",";
    row += gpsField(gps.location.isValid(), String(gps.location.lng(), 6)) + ",";
    row += gpsField(gps.altitude.isValid(), String(gps.altitude.meters(), 1)) + ",";
    row += gpsField(gps.speed.isValid(), String(gps.speed.kmph(), 1)) + ",";
    row += gpsField(gps.course.isValid(), String(gps.course.deg(), 1)) + ",";
    row += String(gpsSatCount()) + ",";
    row += gpsHdopText() + ",";
    row += (gps.location.isValid() ? String(gps.location.age()) : String("--"));
    row += ",";
    row += eventName;
    row += "\r\n";
    return row;
}

static bool saveGpsCsvPoint(const char* path, const char* eventName) {
    if (!gpsFreshFix()) return false;
    if (!ensureGpsCsv(path)) return false;
    return sdAppendTextFile(path, gpsCsvRow(eventName));
}

static void runGpsPosition() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS POSITION");
    beep(1800, 25);

    unsigned long lastDraw = 0;
    unsigned long screenStart = millis();
    while (!isEnterPressed() && !isBackPressed()) {
        drainGps(90);
        if (millis() - lastDraw > 350) {
            tft.fillRect(10, 48, 300, 158, TFT_BLACK);
            bool fix = gps.location.isValid();
            drawStringCustom(12, 52, "FIX: " + String(fix ? "LOCKED" : "WAITING"),
                             fix ? TFT_GREEN : TFT_RED, 2);
            drawStringCustom(12, 82, "Lat: " + gpsField(fix, String(gps.location.lat(), 6)),
                             TFT_WHITE, 1);
            drawStringCustom(12, 100, "Lng: " + gpsField(fix, String(gps.location.lng(), 6)),
                             TFT_WHITE, 1);
            drawStringCustom(12, 118, "Alt: " +
                gpsField(gps.altitude.isValid(), String(gps.altitude.meters(), 1) + " m"),
                TFT_WHITE, 1);
            drawStringCustom(12, 136, "Speed: " +
                gpsField(gps.speed.isValid(), String(gps.speed.kmph(), 1) + " km/h"),
                TFT_WHITE, 1);
            drawStringCustom(12, 154, "Course: " +
                gpsField(gps.course.isValid(), String(gps.course.deg(), 1) + " deg"),
                TFT_WHITE, 1);
            drawStringCustom(12, 180, "Age: " +
                gpsField(fix, String(gps.location.age()) + " ms"), TFT_CYAN, 1);
            lastDraw = millis();
        }
    }
    while (isEnterPressed() || isBackPressed()) delay(5);
}

static void runGpsStats() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS SIGNAL");
    beep(1800, 25);

    unsigned long lastDraw = 0;
    unsigned long screenStart = millis();
    while (!isEnterPressed() && !isBackPressed()) {
        drainGps(90);
        if (millis() - lastDraw > 350) {
            tft.fillRect(10, 48, 300, 158, TFT_BLACK);
            drawStringCustom(12, 54, "Satellites: " +
                gpsField(gps.satellites.isValid(), String(gps.satellites.value())),
                TFT_WHITE, 2);
            drawStringCustom(12, 84, "HDOP: " +
                gpsField(gps.hdop.isValid(), String(gps.hdop.hdop(), 1)),
                TFT_WHITE, 1);
            drawStringCustom(12, 104, "Chars: " + String(gps.charsProcessed()),
                TFT_CYAN, 1);
            drawStringCustom(12, 122, "Sentences OK: " + String(gps.sentencesWithFix()),
                TFT_GREEN, 1);
            drawStringCustom(12, 140, "Checksum fail: " + String(gps.failedChecksum()),
                gps.failedChecksum() ? TFT_YELLOW : TFT_WHITE, 1);
            drawStringCustom(12, 164, "UART RX:" + String(GPS_RX_PIN) +
                " TX:" + String(GPS_TX_PIN) + " " + String(GPS_BAUD), TFT_CYAN, 1);
            if (gps.charsProcessed() == 0 && millis() - screenStart > 3000) {
                drawStringCustom(12, 182, "Sin NMEA: cable TX/VCC/GND o baud", TFT_YELLOW, 1);
            }

            int sats = gps.satellites.isValid() ? gps.satellites.value() : 0;
            int barW = map(sats, 0, 12, 0, 220);
            if (barW > 220) barW = 220;
            tft.drawRect(12, 188, 224, 10, TFT_WHITE);
            tft.fillRect(14, 190, barW, 6, sats >= 4 ? TFT_GREEN : TFT_ORANGE);
            lastDraw = millis();
        }
    }
    while (isEnterPressed() || isBackPressed()) delay(5);
}

static void runGpsConsole() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS NMEA");
    drawStringCustom(10, 222, "BACK: RETURN", TFT_WHITE, 1);
    beep(1800, 25);

    char lines[8][39] = {};
    char current[82] = {};
    uint8_t pos = 0;

    auto pushLine = [&](const char* line) {
        for (int i = 0; i < 7; i++) strncpy(lines[i], lines[i + 1], sizeof(lines[i]));
        strncpy(lines[7], line, sizeof(lines[7]) - 1);
        lines[7][sizeof(lines[7]) - 1] = '\0';
        tft.fillRect(8, 46, 304, 158, TFT_BLACK);
        for (int i = 0; i < 8; i++) {
            drawStringCustom(10, 50 + i * 18, String(lines[i]), i == 7 ? TFT_GREEN : TFT_WHITE, 1);
        }
    };

    unsigned long lastIdleDraw = 0;
    while (!isBackPressed() && !isEnterPressed()) {
        bool got = false;
        while (gpsSerial.available()) {
            char c = (char)gpsSerial.read();
            gps.encode(c);
            got = true;
            if (c == '\n' || c == '\r') {
                if (pos > 0) {
                    current[pos] = '\0';
                    pushLine(current);
                    pos = 0;
                }
            } else if (pos < sizeof(current) - 1 && c >= 32 && c <= 126) {
                current[pos++] = c;
            }
        }
        if (!got && millis() - lastIdleDraw > 1000 && lines[7][0] == '\0') {
            drawStringCustom(34, 100, "Esperando datos NMEA...", TFT_YELLOW, 1);
            lastIdleDraw = millis();
        }
        delay(5);
    }
    while (isEnterPressed() || isBackPressed()) delay(5);
}

static void exportGpsSnapshotReport();

static void drawGpsDashboardBody(const char* statusLine = nullptr) {
    bool fix = gpsFreshFix();
    int score = gpsQualityScore();
    uint16_t fixCol = fix ? TFT_GREEN : TFT_YELLOW;

    tft.fillRect(8, 42, 304, 166, TFT_BLACK);

    drawStringCustom(12, 46, "FIX", TFT_CYAN, 1);
    drawStringBig(52, 42, fix ? "LOCK" : "WAIT", fixCol, 1);
    drawStringCustom(148, 46, "SAT:" + String(gpsSatCount()), TFT_WHITE, 1);
    drawStringCustom(218, 46, "HDOP:" + gpsHdopText(), TFT_WHITE, 1);

    drawGpsQualityBar(12, 66, 128, 10, score);
    drawStringCustom(150, 65, "QUALITY " + String(score) + "%", gpsQualityColor(score), 1);
    drawStringCustom(238, 65, "AGE " + gpsAgeText(), TFT_CYAN, 1);

    drawStringCustom(12, 88, "LAT", TFT_CYAN, 1);
    drawStringFit(52, 88, gpsField(fix, String(gps.location.lat(), 6)), TFT_WHITE, 118, 1);
    drawStringCustom(178, 88, "LNG", TFT_CYAN, 1);
    drawStringFit(218, 88, gpsField(fix, String(gps.location.lng(), 6)), TFT_WHITE, 92, 1);

    drawStringCustom(12, 112, "ALT", TFT_CYAN, 1);
    drawStringFit(52, 112,
                  gpsField(gps.altitude.isValid(), String(gps.altitude.meters(), 1) + "m"),
                  TFT_WHITE, 94, 1);
    drawStringCustom(160, 112, "SPD", TFT_CYAN, 1);
    drawStringFit(202, 112,
                  gpsField(gps.speed.isValid(), String(gps.speed.kmph(), 1) + "km/h"),
                  TFT_WHITE, 108, 1);

    String courseLine = "--";
    if (gps.course.isValid()) {
        courseLine = String(gps.course.deg(), 1) + " " + gpsCardinal(gps.course.deg());
    }
    drawStringCustom(12, 136, "CRS", TFT_CYAN, 1);
    drawStringFit(52, 136, courseLine, TFT_WHITE, 100, 1);
    drawStringCustom(160, 136, "UTC", TFT_CYAN, 1);
    drawStringFit(202, 136, gpsUtcStamp(), TFT_WHITE, 108, 1);

    if (gps.charsProcessed() == 0) {
        drawStringFit(12, 164, "No NMEA yet: check GPS power, TX->GPIO18 and baud",
                      TFT_YELLOW, 296, 1);
    } else if (!fix) {
        drawStringFit(12, 164, "Waiting for fresh fix. Try outdoors or near a window.",
                      TFT_YELLOW, 296, 1);
    } else {
        drawStringFit(12, 164, "Fresh location ready. OK saves a waypoint to SD.",
                      TFT_GREEN, 296, 1);
    }

    if (statusLine && statusLine[0]) {
        tft.fillRect(12, 190, 296, 14, TFT_BLACK);
        drawStringFit(12, 190, String(statusLine), TFT_CYAN, 296, 1);
    }
}

static void runGpsProDashboard() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS PRO");
    drawStringCustom(8, 222, "OK:MARK  BACK/OK(H):EXIT", TFT_WHITE, 1);
    beep(1800, 25);

    unsigned long lastDraw = 0;
    char status[42] = "";
    bool exitTool = false;

    while (!exitTool) {
        drainGps(70);

        NavAction action = readNavAction(120);
        if (action == NAV_BACK) {
            exitTool = true;
        } else if (action == NAV_ENTER) {
            bool held = waitOkReleaseWasLong();
            if (held) {
                exitTool = true;
            } else {
                bool ok = saveGpsCsvPoint("/GPS_MARKS.csv", "DASH_MARK");
                snprintf(status, sizeof(status), ok ? "Saved /GPS_MARKS.csv" : "No fresh fix or SD error");
                beep(ok ? 2400 : 900, 45);
                lastDraw = 0;
                flushNavInput();
            }
        }

        if (millis() - lastDraw > 350) {
            drawGpsDashboardBody(status);
            lastDraw = millis();
        }
        delay(8);
    }

    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput();
}

static void drawGpsCompassFace(int cx, int cy, int r, bool validCourse, double deg) {
    tft.drawCircle(cx, cy, r, TFT_WHITE);
    tft.drawCircle(cx, cy, r - 12, UI_ACCENT);
    tft.drawFastVLine(cx, cy - r, 8, TFT_WHITE);
    tft.drawFastVLine(cx, cy + r - 8, 8, TFT_WHITE);
    tft.drawFastHLine(cx - r, cy, 8, TFT_WHITE);
    tft.drawFastHLine(cx + r - 8, cy, 8, TFT_WHITE);
    drawStringCustom(cx - 3, cy - r + 12, "N", TFT_RED, 1);
    drawStringCustom(cx + r - 14, cy - 3, "E", TFT_WHITE, 1);
    drawStringCustom(cx - 3, cy + r - 20, "S", TFT_WHITE, 1);
    drawStringCustom(cx - r + 9, cy - 3, "W", TFT_WHITE, 1);

    if (!validCourse) {
        drawStringCustom(cx - 26, cy - 4, "MOVE", TFT_YELLOW, 1);
        return;
    }

    float rad = deg * 3.14159265f / 180.0f;
    int x2 = cx + (int)(sin(rad) * (r - 18));
    int y2 = cy - (int)(cos(rad) * (r - 18));
    tft.drawLine(cx, cy, x2, y2, TFT_GREEN);
    tft.drawLine(cx + 1, cy, x2 + 1, y2, TFT_GREEN);
    tft.fillCircle(x2, y2, 3, TFT_GREEN);
    tft.fillCircle(cx, cy, 3, TFT_WHITE);
}

static void runGpsCompass() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS COMPASS");
    drawStringCustom(8, 222, "BACK/OK(H):EXIT", TFT_WHITE, 1);
    beep(1800, 25);

    unsigned long lastDraw = 0;
    bool exitTool = false;

    while (!exitTool) {
        drainGps(70);
        NavAction action = readNavAction(120);
        if (action == NAV_BACK) exitTool = true;
        if (action == NAV_ENTER && waitOkReleaseWasLong()) exitTool = true;

        if (millis() - lastDraw > 300) {
            bool validCourse = gps.course.isValid() && gps.speed.isValid() && gps.speed.kmph() > 1.0;
            double deg = gps.course.isValid() ? gps.course.deg() : 0.0;
            tft.fillRect(8, 42, 304, 166, TFT_BLACK);
            drawGpsCompassFace(82, 124, 54, validCourse, deg);

            drawStringCustom(154, 52, "COURSE", TFT_CYAN, 1);
            drawStringBig(154, 70,
                          validCourse ? String((int)(deg + 0.5)) : String("--"),
                          validCourse ? TFT_GREEN : TFT_YELLOW, 2);
            drawStringCustom(230, 82,
                             validCourse ? String(gpsCardinal(deg)) : String("--"),
                             validCourse ? TFT_GREEN : TFT_YELLOW, 2);
            drawStringCustom(154, 118, "Speed: " +
                gpsField(gps.speed.isValid(), String(gps.speed.kmph(), 1) + " km/h"),
                TFT_WHITE, 1);
            drawStringCustom(154, 138, "Sat: " + String(gpsSatCount()) +
                "  HDOP:" + gpsHdopText(), TFT_WHITE, 1);
            drawStringFit(154, 164,
                validCourse ? "Course from GPS movement." : "Move forward for GPS course.",
                UI_ACCENT, 150, 1);
            lastDraw = millis();
        }
        delay(8);
    }

    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput();
}

static void runGpsTrackLogger() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS LOGGER");
    drawStringCustom(8, 222, "OK:START/PAUSE  UP/DN:RATE  BACK", TFT_WHITE, 1);
    beep(1800, 25);

    const int intervals[] = { 1, 2, 5, 10, 30 };
    int intervalIdx = 2;
    bool logging = false;
    bool sdOk = ensureGpsCsv("/GPS_TRACK.csv");
    unsigned long lastDraw = 0;
    unsigned long lastSave = 0;
    int saved = 0;
    int skipped = 0;
    char status[44] = "";

    if (!sdOk) strncpy(status, "SD error: cannot create GPS_TRACK.csv", sizeof(status) - 1);
    else strncpy(status, "Ready: OK starts logging", sizeof(status) - 1);
    status[sizeof(status) - 1] = '\0';

    bool exitTool = false;
    while (!exitTool) {
        drainGps(70);
        NavAction action = readNavAction(120);

        if (action == NAV_BACK) {
            exitTool = true;
        } else if (action == NAV_ENTER) {
            bool held = waitOkReleaseWasLong();
            if (held) {
                exitTool = true;
            } else if (sdOk) {
                logging = !logging;
                snprintf(status, sizeof(status), logging ? "Logging active" : "Logging paused");
                beep(logging ? 2400 : 900, 40);
                flushNavInput();
            }
        } else if (action == NAV_UP || action == NAV_DOWN) {
            intervalIdx += (action == NAV_UP) ? 1 : -1;
            if (intervalIdx < 0) intervalIdx = 4;
            if (intervalIdx > 4) intervalIdx = 0;
            snprintf(status, sizeof(status), "Interval %ds", intervals[intervalIdx]);
            beep(2200, 15);
        }

        if (logging && sdOk && millis() - lastSave >= (unsigned long)intervals[intervalIdx] * 1000UL) {
            if (saveGpsCsvPoint("/GPS_TRACK.csv", "TRACK")) {
                saved++;
                snprintf(status, sizeof(status), "Saved point #%d", saved);
                beep(2600, 12);
            } else {
                skipped++;
                snprintf(status, sizeof(status), "Skipped: no fresh fix");
            }
            lastSave = millis();
            lastDraw = 0;
        }

        if (millis() - lastDraw > 350) {
            tft.fillRect(8, 42, 304, 166, TFT_BLACK);
            drawStringCustom(12, 48, logging ? "REC" : "PAUSED",
                             logging ? TFT_GREEN : TFT_YELLOW, 2);
            drawStringCustom(112, 52, "/GPS_TRACK.csv", sdOk ? TFT_CYAN : TFT_RED, 1);
            drawStringCustom(12, 80, "Interval: " + String(intervals[intervalIdx]) + "s",
                             TFT_WHITE, 1);
            drawStringCustom(150, 80, "Saved: " + String(saved), TFT_GREEN, 1);
            drawStringCustom(230, 80, "Skip: " + String(skipped), skipped ? TFT_YELLOW : TFT_WHITE, 1);

            drawStringCustom(12, 108, "Fix: " + String(gpsFreshFix() ? "YES" : "NO"),
                             gpsFreshFix() ? TFT_GREEN : TFT_YELLOW, 1);
            drawStringCustom(112, 108, "Sat:" + String(gpsSatCount()) + " HDOP:" + gpsHdopText(),
                             TFT_WHITE, 1);
            drawStringFit(12, 132,
                          "Lat " + gpsField(gpsFreshFix(), String(gps.location.lat(), 6)),
                          TFT_WHITE, 296, 1);
            drawStringFit(12, 150,
                          "Lng " + gpsField(gpsFreshFix(), String(gps.location.lng(), 6)),
                          TFT_WHITE, 296, 1);
            drawStringFit(12, 178, String(status), sdOk ? TFT_CYAN : TFT_RED, 296, 1);
            lastDraw = millis();
        }

        delay(8);
    }

    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput();
}

static void runGpsWaypointMarker() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS MARK");
    drawStringCustom(8, 222, "OK:SAVE MARK  BACK/OK(H):EXIT", TFT_WHITE, 1);
    beep(1800, 25);

    unsigned long lastDraw = 0;
    int marks = 0;
    char status[44] = "Waiting for GPS fix";
    bool exitTool = false;

    while (!exitTool) {
        drainGps(70);
        NavAction action = readNavAction(120);
        if (action == NAV_BACK) {
            exitTool = true;
        } else if (action == NAV_ENTER) {
            bool held = waitOkReleaseWasLong();
            if (held) {
                exitTool = true;
            } else {
                bool ok = saveGpsCsvPoint("/GPS_MARKS.csv", "WAYPOINT");
                if (ok) {
                    marks++;
                    snprintf(status, sizeof(status), "Saved waypoint #%d", marks);
                } else {
                    snprintf(status, sizeof(status), "No fresh fix or SD error");
                }
                beep(ok ? 2400 : 900, 45);
                lastDraw = 0;
                flushNavInput();
            }
        }

        if (millis() - lastDraw > 350) {
            tft.fillRect(8, 42, 304, 166, TFT_BLACK);
            drawStringCustom(12, 48, "Target file:", TFT_CYAN, 1);
            drawStringCustom(112, 48, "/GPS_MARKS.csv", TFT_WHITE, 1);
            drawStringCustom(12, 74, "Fix: " + String(gpsFreshFix() ? "READY" : "WAIT"),
                             gpsFreshFix() ? TFT_GREEN : TFT_YELLOW, 2);
            drawStringCustom(12, 110, "Sat:" + String(gpsSatCount()) +
                " HDOP:" + gpsHdopText() + " Age:" + gpsAgeText(), TFT_WHITE, 1);
            drawStringFit(12, 136,
                          "Lat " + gpsField(gpsFreshFix(), String(gps.location.lat(), 6)),
                          TFT_WHITE, 296, 1);
            drawStringFit(12, 154,
                          "Lng " + gpsField(gpsFreshFix(), String(gps.location.lng(), 6)),
                          TFT_WHITE, 296, 1);
            drawStringFit(12, 184, String(status), TFT_CYAN, 296, 1);
            lastDraw = millis();
        }
        delay(8);
    }

    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput();
}

void runGpsTools() {
    beginGpsPort();
    static const char* gpsItems[] = {
        "Dashboard Pro",
        "Track Logger",
        "Compass",
        "Waypoint Mark",
        "Live Status",
        "Position",
        "Signal Stats",
        "NMEA Console",
        "Export Snapshot"
    };

    bool exitSub = false;
    while (!exitSub) {
        int choice = runSubMenu("GPS TOOLS PRO", gpsItems, sizeof(gpsItems) / sizeof(char*));
        switch (choice) {
            case -1: exitSub = true;  break;
            case  0: runGpsProDashboard(); break;
            case  1: runGpsTrackLogger();  break;
            case  2: runGpsCompass();      break;
            case  3: runGpsWaypointMarker(); break;
            case  4: runGpsStatus();       break;
            case  5: runGpsPosition();     break;
            case  6: runGpsStats();        break;
            case  7: runGpsConsole();      break;
            case  8: exportGpsSnapshotReport(); break;
        }
    }
}

void runGpsStatus() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    drawToolFrame("GPS STATUS");
    beep(1800, 25);

    unsigned long lastDraw = 0;
    unsigned long screenStart = millis();
    while (!isEnterPressed() && !isBackPressed()) {
        drainGps(80);
        if (millis() - lastDraw > 300) {
            tft.fillRect(10, 48, 300, 158, TFT_BLACK);
            drawStringCustom(12, 52, "UART1 RX:" + String(GPS_RX_PIN) +
                " TX:" + String(GPS_TX_PIN), TFT_CYAN, 1);
            drawStringCustom(12, 72, "Chars: " + String(gps.charsProcessed()),
                gps.charsProcessed() ? TFT_WHITE : TFT_YELLOW, 1);
            drawStringCustom(12, 90, "Satellites: " +
                (gps.satellites.isValid() ? String(gps.satellites.value()) : String("--")),
                TFT_WHITE, 1);
            drawStringCustom(12, 108, "Fix: " +
                String(gps.location.isValid() ? "YES" : "NO"), gps.location.isValid() ? TFT_GREEN : TFT_RED, 1);
            if (gps.location.isValid()) {
                drawStringCustom(12, 128, "Lat: " + String(gps.location.lat(), 6), TFT_WHITE, 1);
                drawStringCustom(12, 146, "Lng: " + String(gps.location.lng(), 6), TFT_WHITE, 1);
            }
            if (gps.time.isValid()) {
                char buf[20];
                snprintf(buf, sizeof(buf), "%02d:%02d:%02d UTC",
                         gps.time.hour(), gps.time.minute(), gps.time.second());
                drawStringCustom(12, 166, "Time: " + String(buf), TFT_WHITE, 1);
            }
            if (gps.charsProcessed() == 0 && millis() - screenStart > 3000) {
                drawStringCustom(12, 188, "Sin datos UART: revisa VCC/GND/TX->GPIO18",
                    TFT_YELLOW, 1);
            }
            lastDraw = millis();
        }
    }
    while (isEnterPressed() || isBackPressed()) delay(5);
}

void runSdStatus() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    drawToolFrame("MICROSD");
    beep(1800, 25);

    bool ok = beginSd();
    tft.fillRect(10, 52, 300, 145, TFT_BLACK);
    drawStringCustom(12, 56, "SPI SCK:" + String(SD_SCK_PIN) +
        " MOSI:" + String(SD_MOSI_PIN), TFT_CYAN, 1);
    drawStringCustom(12, 72, "MISO:" + String(SD_MISO_PIN) +
        " CS:" + String(SD_CS_PIN), TFT_CYAN, 1);

    if (!ok) {
        drawStringBig(35, 105, "SD ERROR", TFT_RED, 2);
        drawStringCustom(20, 145, "Revisa formato FAT32/cableado", TFT_WHITE, 1);
    } else {
        uint8_t type = SD.cardType();
        uint64_t sizeMb = SD.cardSize() / (1024ULL * 1024ULL);
        uint64_t usedMb = SD.usedBytes() / (1024ULL * 1024ULL);
        const char* typeName = type == CARD_MMC ? "MMC" :
                               type == CARD_SD ? "SDSC" :
                               type == CARD_SDHC ? "SDHC" : "UNKNOWN";
        drawStringCustom(12, 100, "Status: OK", TFT_GREEN, 2);
        drawStringCustom(12, 126, "Type: " + String(typeName), TFT_WHITE, 1);
        drawStringCustom(12, 144, "Size: " + String((unsigned long)sizeMb) + " MB", TFT_WHITE, 1);
        drawStringCustom(12, 162, "Used: " + String((unsigned long)usedMb) + " MB", TFT_WHITE, 1);
        drawStringCustom(12, 180, "SPI: " + String((unsigned long)(sdMountHz / 1000000)) + " MHz", TFT_CYAN, 1);
    }

    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
}

struct SdBrowserEntry {
    char name[34];
    bool dir;
    uint32_t size;
};

static const int SD_MAX_ENTRIES = 48;
static SdBrowserEntry sdEntries[SD_MAX_ENTRIES];
static int sdEntryCount = 0;

static const char* baseName(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void joinPath(const char* dir, const char* name, char* out, size_t outLen) {
    if (strcmp(dir, "/") == 0) snprintf(out, outLen, "/%s", name);
    else snprintf(out, outLen, "%s/%s", dir, name);
}

static bool loadSdEntries(const char* path) {
    sdEntryCount = 0;
    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return false;
    }

    File file = root.openNextFile();
    while (file && sdEntryCount < SD_MAX_ENTRIES) {
        const char* name = baseName(file.name());
        if (name[0] != '.' && name[0] != '\0') {
            strncpy(sdEntries[sdEntryCount].name, name, sizeof(sdEntries[sdEntryCount].name) - 1);
            sdEntries[sdEntryCount].name[sizeof(sdEntries[sdEntryCount].name) - 1] = '\0';
            sdEntries[sdEntryCount].dir = file.isDirectory();
            sdEntries[sdEntryCount].size = file.isDirectory() ? 0 : (uint32_t)file.size();
            sdEntryCount++;
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    for (int i = 0; i < sdEntryCount - 1; i++) {
        for (int j = 0; j < sdEntryCount - 1 - i; j++) {
            bool swapIt = false;
            if (sdEntries[j].dir != sdEntries[j + 1].dir) {
                swapIt = !sdEntries[j].dir && sdEntries[j + 1].dir;
            } else {
                swapIt = strcasecmp(sdEntries[j].name, sdEntries[j + 1].name) > 0;
            }
            if (swapIt) {
                SdBrowserEntry tmp = sdEntries[j];
                sdEntries[j] = sdEntries[j + 1];
                sdEntries[j + 1] = tmp;
            }
        }
    }
    return true;
}

static bool looksTextFile(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return true;
    String ext = String(dot);
    ext.toLowerCase();
    return ext == ".txt" || ext == ".log" || ext == ".csv" || ext == ".json" ||
           ext == ".ini" || ext == ".cfg" || ext == ".md" || ext == ".nmea" ||
           ext == ".gps";
}

static constexpr int SD_BROWSER_VISIBLE = 6;
static constexpr int SD_BROWSER_ROW_H = 24;
static constexpr int SD_BROWSER_LIST_Y = 54;

static void drawSdBrowserFrame(const char* path) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(10, 8, "MICROSD FILES", TFT_WHITE, 1);
    drawStringFit(10, 36, String(path), TFT_CYAN, 300, 1);
    tft.drawFastHLine(0, 50, 320, TFT_WHITE);
    tft.drawFastHLine(0, 214, 320, TFT_WHITE);
    drawStringCustom(8, 222, "OK:OPEN  OK(H)/BACK:UP  ENC:MOVE", TFT_WHITE, 1);
}

static void drawSdBrowserList(int cursor, int scroll) {
    if (sdEntryCount == 0) {
        tft.fillRect(8, SD_BROWSER_LIST_Y, 304,
                     SD_BROWSER_VISIBLE * SD_BROWSER_ROW_H, TFT_BLACK);
        drawStringCustom(70, 112, "Carpeta vacia", TFT_YELLOW, 1);
        tft.fillRect(315, SD_BROWSER_LIST_Y, 3,
                     SD_BROWSER_VISIBLE * SD_BROWSER_ROW_H, TFT_BLACK);
        return;
    }

    for (int row = 0; row < SD_BROWSER_VISIBLE; row++) {
        int idx = scroll + row;
        int y = SD_BROWSER_LIST_Y + row * SD_BROWSER_ROW_H;
        tft.fillRect(8, y - 2, 304, SD_BROWSER_ROW_H - 2, TFT_BLACK);
        if (idx >= sdEntryCount) continue;

        bool sel = idx == cursor;
        uint16_t bg = sel ? TFT_WHITE : TFT_BLACK;
        uint16_t fg = sel ? TFT_BLACK : TFT_WHITE;
        uint16_t sub = sel ? TFT_BLACK : TFT_CYAN;

        tft.fillRect(8, y - 2, 304, SD_BROWSER_ROW_H - 2, bg);
        tft.drawRect(8, y - 2, 304, SD_BROWSER_ROW_H - 2, TFT_WHITE);
        String prefix = sdEntries[idx].dir ? "[D] " : "[F] ";
        drawStringFit(16, y + 3, prefix + String(sdEntries[idx].name), fg, 210, 1);
        if (!sdEntries[idx].dir) {
            String size = String((unsigned long)sdEntries[idx].size) + "B";
            drawStringRight(302, y + 3, size, sub, 1);
        }
    }

    int trackH = SD_BROWSER_VISIBLE * SD_BROWSER_ROW_H;
    tft.fillRect(315, SD_BROWSER_LIST_Y, 3, trackH, TFT_BLACK);
    if (sdEntryCount > SD_BROWSER_VISIBLE) {
        int barH = (SD_BROWSER_VISIBLE * trackH) / sdEntryCount;
        if (barH < 8) barH = 8;
        int barY = SD_BROWSER_LIST_Y + (scroll * (trackH - barH)) /
                   (sdEntryCount - SD_BROWSER_VISIBLE);
        tft.fillRect(315, barY, 3, barH, TFT_CYAN);
    }
}

static void drawSdBrowser(const char* path, int cursor, int scroll, bool fullRedraw) {
    tft.startWrite();
    if (fullRedraw) drawSdBrowserFrame(path);
    drawSdBrowserList(cursor, scroll);
    tft.endWrite();
}

static void showSdErrorScreen(const char* title) {
    drawToolFrame(title);
    drawStringBig(35, 95, "SD ERROR", TFT_RED, 2);
    drawStringCustom(20, 132, "No se pudo montar la tarjeta", TFT_WHITE, 1);
    drawStringCustom(20, 150, "SCK:" + String(SD_SCK_PIN) + " MOSI:" + String(SD_MOSI_PIN) +
        " MISO:" + String(SD_MISO_PIN) + " CS:" + String(SD_CS_PIN), TFT_CYAN, 1);
    drawStringCustom(20, 168, "Prueba reinsertar o reiniciar", TFT_YELLOW, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput();
}

static void drawSdActionResult(const char* title, bool ok, const String& line1, const String& line2 = "") {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, ok ? TFT_GREEN : TFT_RED);
    drawStringBig(10, 8, title, ok ? TFT_GREEN : TFT_RED, 1);
    tft.drawFastHLine(0, 34, 320, ok ? TFT_GREEN : TFT_RED);
    drawStringFit(18, 82, line1, ok ? TFT_CYAN : TFT_YELLOW, 284, 2);
    if (line2.length()) drawStringFit(18, 128, line2, TFT_WHITE, 284, 1);
    tft.drawFastHLine(0, 214, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK/BACK: RETURN", UI_ACCENT, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput();
}

static void runSdTextViewer(const char* path) {
    if (!looksTextFile(path)) {
        drawToolFrame("FILE VIEWER");
        drawStringCustom(20, 92, "Tipo de archivo no reconocido.", TFT_YELLOW, 1);
        drawStringCustom(20, 114, "Se intentara abrir como texto.", TFT_WHITE, 1);
        delay(800);
    }

    File file = SD.open(path, FILE_READ);
    if (!file) {
        drawToolFrame("FILE ERROR");
        drawStringCustom(28, 110, "No se pudo abrir el archivo", TFT_RED, 1);
        while (!isEnterPressed() && !isBackPressed()) delay(10);
        while (isEnterPressed() || isBackPressed()) delay(5);
        return;
    }

    const int maxPages = 64;
    uint32_t pageStarts[maxPages] = {};
    int page = 0;
    int knownPages = 1;
    bool exitViewer = false;

    while (!exitViewer) {
        file.seek(pageStarts[page]);
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, TFT_WHITE);
        drawStringBig(10, 8, "FILE VIEW", TFT_WHITE, 1);
        drawStringFit(10, 34, String(baseName(path)), TFT_CYAN, 300, 1);
        tft.drawFastHLine(0, 50, 320, TFT_WHITE);
        tft.drawFastHLine(0, 214, 320, TFT_WHITE);
        drawStringCustom(8, 222, "DN/OK:NEXT  UP:PREV  BACK", TFT_WHITE, 1);

        char line[40];
        int linePos = 0;
        int drawn = 0;
        while (file.available() && drawn < 9) {
            char c = (char)file.read();
            if (c == '\r') continue;
            if (c == '\n' || linePos >= 38) {
                line[linePos] = '\0';
                drawStringFit(10, 58 + drawn * 17, String(line), TFT_WHITE, 300, 1);
                drawn++;
                linePos = 0;
                if (c != '\n') {
                    while (file.available()) {
                        char skip = (char)file.read();
                        if (skip == '\n') break;
                    }
                }
            } else {
                line[linePos++] = (c >= 32 && c <= 126) ? c : '.';
            }
        }
        if (linePos > 0 && drawn < 9) {
            line[linePos] = '\0';
            drawStringFit(10, 58 + drawn * 17, String(line), TFT_WHITE, 300, 1);
            drawn++;
        }
        if (drawn == 0) drawStringCustom(94, 116, "<fin de archivo>", TFT_YELLOW, 1);

        uint32_t nextPos = file.position();
        if (page + 1 < maxPages && nextPos > pageStarts[page]) {
            pageStarts[page + 1] = nextPos;
            if (knownPages < page + 2) knownPages = page + 2;
        }

        bool waitAction = true;
        while (waitAction) {
            NavAction action = readNavAction(120);
            if (action == NAV_BACK) {
                exitViewer = true;
                waitAction = false;
            } else if (action == NAV_ENTER) {
                bool held = waitOkReleaseWasLong();
                if (held) {
                    exitViewer = true;
                } else if (file.available() && page + 1 < maxPages) {
                    page++;
                }
                waitAction = false;
            } else if (action == NAV_DOWN && file.available() && page + 1 < maxPages) {
                page++;
                waitAction = false;
            } else if (action == NAV_UP && page > 0) {
                page--;
                waitAction = false;
            }
            delay(8);
        }
    }

    file.close();
    while (isEnterPressed() || isBackPressed()) delay(5);
}

static void runSdBrowserTool() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beep(1800, 25);

    if (!beginSd()) {
        showSdErrorScreen("MICROSD FILES");
        return;
    }

    char path[128] = "/";
    int cursor = 0;
    int scroll = 0;
    bool exitBrowser = false;

    auto reload = [&]() {
        loadSdEntries(path);
        if (cursor >= sdEntryCount) cursor = sdEntryCount - 1;
        if (cursor < 0) cursor = 0;
        scroll = 0;
        drawSdBrowser(path, cursor, scroll, true);
    };

    auto goBack = [&]() {
        if (strcmp(path, "/") == 0) {
            exitBrowser = true;
        } else {
            char* slash = strrchr(path, '/');
            if (slash && slash != path) *slash = '\0';
            else strcpy(path, "/");
            cursor = 0;
            reload();
        }
    };

    reload();
    while (!exitBrowser) {
        NavAction action = readNavAction(120);
        if ((action == NAV_UP || action == NAV_DOWN) && sdEntryCount > 0) {
            cursor += (action == NAV_DOWN) ? 1 : -1;
            if (cursor < 0) cursor = sdEntryCount - 1;
            if (cursor >= sdEntryCount) cursor = 0;
            if (cursor < scroll) scroll = cursor;
            if (cursor >= scroll + SD_BROWSER_VISIBLE) scroll = cursor - SD_BROWSER_VISIBLE + 1;
            beep(2200, 12);
            drawSdBrowser(path, cursor, scroll, false);
        } else if (action == NAV_ENTER && sdEntryCount > 0) {
            bool held = waitOkReleaseWasLong();
            if (held) {
                goBack();
                delay(80);
                continue;
            }
            char nextPath[128];
            joinPath(path, sdEntries[cursor].name, nextPath, sizeof(nextPath));
            if (sdEntries[cursor].dir) {
                strncpy(path, nextPath, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
                cursor = 0;
                reload();
            } else {
                runSdTextViewer(nextPath);
                drawSdBrowser(path, cursor, scroll, true);
            }
        } else if (action == NAV_BACK) {
            goBack();
        }
        delay(8);
    }

    while (isEnterPressed() || isBackPressed()) delay(5);
}

static String sdSizeText(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024ULL) {
        return String((unsigned long)(bytes / (1024ULL * 1024ULL))) + "MB";
    }
    if (bytes >= 1024ULL) {
        return String((unsigned long)(bytes / 1024ULL)) + "KB";
    }
    return String((unsigned long)bytes) + "B";
}

static const char* const SD_REPORT_PATHS[] = {
    "/GPS_TRACK.csv",
    "/GPS_MARKS.csv",
    "/GPS_SNAPSHOT.txt",
    "/THREAT_REPORT.txt",
    "/WIFI_DEFENSE.txt",
    "/WIFI_AUDIT.csv",
    "/RF_BASELINE.txt",
    "/BLE_AUDIT.txt",
    "/BATTERY_STATUS.txt",
    "/SD_INDEX.txt"
};

static const uint8_t SD_REPORT_COUNT = sizeof(SD_REPORT_PATHS) / sizeof(SD_REPORT_PATHS[0]);

static void runSdReportViewer() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    if (!beginSd()) {
        showSdErrorScreen("SD REPORTS");
        return;
    }

    char labels[SD_REPORT_COUNT][34];
    const char* items[SD_REPORT_COUNT];
    const char* paths[SD_REPORT_COUNT];
    int count = 0;

    for (uint8_t i = 0; i < SD_REPORT_COUNT; i++) {
        if (!SD.exists(SD_REPORT_PATHS[i])) continue;
        File f = SD.open(SD_REPORT_PATHS[i], FILE_READ);
        uint32_t size = f ? (uint32_t)f.size() : 0;
        if (f) f.close();
        snprintf(labels[count], sizeof(labels[count]), "%s %s",
                 baseName(SD_REPORT_PATHS[i]), sdSizeText(size).c_str());
        items[count] = labels[count];
        paths[count] = SD_REPORT_PATHS[i];
        count++;
    }

    if (count == 0) {
        drawSdActionResult("SD REPORTS", false, "No hay reportes", "Genera logs o auditorias primero.");
        return;
    }

    bool exitViewer = false;
    while (!exitViewer) {
        int choice = runSubMenu("SD REPORTS", items, count);
        if (choice < 0) exitViewer = true;
        else runSdTextViewer(paths[choice]);
    }
}

static void runSdCreateFolders() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    if (!beginSd()) {
        showSdErrorScreen("SD FOLDERS");
        return;
    }

    static const char* const folders[] = {
        "/GPS",
        "/REPORTS",
        "/LOGS",
        "/EXPORTS",
        "/CAPTURES"
    };

    String detail;
    bool ok = true;
    for (uint8_t i = 0; i < sizeof(folders) / sizeof(folders[0]); i++) {
        if (SD.exists(folders[i])) {
            detail += String(folders[i]) + " OK ";
            continue;
        }
        bool made = SD.mkdir(folders[i]);
        ok = ok && made;
        detail += String(folders[i]) + (made ? " OK " : " ERR ");
    }

    drawSdActionResult("SD FOLDERS", ok, ok ? "Carpetas listas" : "Algunas fallaron", detail);
}

static void appendDirectoryIndex(File& dir, const String& path, String& out, int depth,
                                 int& files, int& dirs, uint64_t& bytes) {
    if (depth > 3 || files + dirs >= 120 || out.length() > 10000) return;

    File entry = dir.openNextFile();
    while (entry && files + dirs < 120 && out.length() <= 10000) {
        String name = String(baseName(entry.name()));
        if (!name.startsWith(".")) {
            String fullPath = (path == "/") ? "/" + name : path + "/" + name;
            if (entry.isDirectory()) {
                dirs++;
                out += "[DIR]  " + fullPath + "\r\n";
                appendDirectoryIndex(entry, fullPath, out, depth + 1, files, dirs, bytes);
            } else {
                uint64_t size = entry.size();
                files++;
                bytes += size;
                out += "[FILE] " + fullPath + "  " + sdSizeText(size) + "\r\n";
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
}

static void runSdExportIndex() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    if (!beginSd()) {
        showSdErrorScreen("SD INDEX");
        return;
    }

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        drawSdActionResult("SD INDEX", false, "No se pudo abrir /");
        return;
    }

    String out;
    out.reserve(4096);
    out += "CYBERDECK MICROSD INDEX\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += "CardMB: " + String((unsigned long)(SD.cardSize() / (1024ULL * 1024ULL))) + "\r\n";
    out += "UsedMB: " + String((unsigned long)(SD.usedBytes() / (1024ULL * 1024ULL))) + "\r\n";
    out += "\r\nFILES\r\n";

    int files = 0;
    int dirs = 0;
    uint64_t bytes = 0;
    appendDirectoryIndex(root, "/", out, 0, files, dirs, bytes);
    root.close();

    out += "\r\nSUMMARY\r\n";
    out += "Dirs: " + String(dirs) + "\r\n";
    out += "Files: " + String(files) + "\r\n";
    out += "ListedBytes: " + String((unsigned long)bytes) + "\r\n";
    if (files + dirs >= 120) out += "Note: listing truncated at 120 entries.\r\n";

    bool ok = sdWriteTextFile("/SD_INDEX.txt", out);
    drawSdActionResult("SD INDEX", ok, ok ? "/SD_INDEX.txt" : "No se pudo guardar",
                       ok ? ("Files " + String(files) + " Dirs " + String(dirs)) : "Revisa SD/espacio.");
}

static bool confirmSdCleanup() {
    drawToolFrame("CLEAN REPORTS");
    drawStringFit(16, 58, "Esto borra solo reportes generados en raiz.",
                  TFT_YELLOW, 288, 1);
    drawStringFit(16, 82, "No borra carpetas ni archivos desconocidos.",
                  TFT_WHITE, 288, 1);
    drawStringFit(16, 118, "Mantener OK: borrar reportes",
                  TFT_RED, 288, 1);
    drawStringFit(16, 142, "BACK: cancelar",
                  TFT_CYAN, 288, 1);
    drawStringCustom(10, 222, "OK(HOLD): CONFIRM  BACK:CANCEL", UI_ACCENT, 1);

    while (true) {
        NavAction action = readNavAction(120);
        if (action == NAV_BACK) return false;
        if (action == NAV_ENTER) {
            bool held = waitOkReleaseWasLong(900);
            flushNavInput();
            return held;
        }
        delay(8);
    }
}

static void runSdCleanReports() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    if (!beginSd()) {
        showSdErrorScreen("CLEAN REPORTS");
        return;
    }

    if (!confirmSdCleanup()) {
        drawSdActionResult("CLEAN REPORTS", true, "Cancelado", "No se borro nada.");
        return;
    }

    int removed = 0;
    int failed = 0;
    for (uint8_t i = 0; i < SD_REPORT_COUNT; i++) {
        if (!SD.exists(SD_REPORT_PATHS[i])) continue;
        if (SD.remove(SD_REPORT_PATHS[i])) removed++;
        else failed++;
    }

    bool ok = failed == 0;
    drawSdActionResult("CLEAN REPORTS", ok,
                       "Borrados: " + String(removed),
                       failed ? ("Fallos: " + String(failed)) : "Reportes raiz limpios.");
}

void runSdFileBrowser() {
    static const char* managerItems[] = {
        "Browse Files",
        "Quick Reports",
        "Create Folders",
        "Export SD Index",
        "Clean Reports",
        "SD Info"
    };

    bool exitManager = false;
    while (!exitManager) {
        int choice = runSubMenu("MICROSD MANAGER", managerItems,
                                sizeof(managerItems) / sizeof(char*));
        switch (choice) {
            case -1: exitManager = true;   break;
            case  0: runSdBrowserTool();   break;
            case  1: runSdReportViewer();  break;
            case  2: runSdCreateFolders(); break;
            case  3: runSdExportIndex();   break;
            case  4: runSdCleanReports();  break;
            case  5: runSdStatus();        break;
        }
    }
}

#define BAT_R_TOP_OHMS     2200UL
#define BAT_R_BOTTOM_OHMS  1000UL
#define BAT_SAMPLES        24

static int batteryPercentFromMv(uint32_t mv) {
    struct Point { uint16_t mv; uint8_t pct; };
    static const Point curve[] = {
        { 4200, 100 }, { 4110, 90 }, { 4030, 80 }, { 3970, 70 },
        { 3910, 60 },  { 3850, 50 }, { 3790, 40 }, { 3750, 30 },
        { 3710, 20 },  { 3650, 10 }, { 3300, 0 }
    };

    if (mv >= curve[0].mv) return 100;
    int last = sizeof(curve) / sizeof(curve[0]) - 1;
    if (mv <= curve[last].mv) return 0;

    for (int i = 0; i < last; i++) {
        if (mv <= curve[i].mv && mv >= curve[i + 1].mv) {
            int highMv = curve[i].mv;
            int lowMv = curve[i + 1].mv;
            int highPct = curve[i].pct;
            int lowPct = curve[i + 1].pct;
            return lowPct + ((int)(mv - lowMv) * (highPct - lowPct)) / (highMv - lowMv);
        }
    }
    return 0;
}

static uint16_t batteryColor(int pct) {
    if (pct < 0) return TFT_CYAN;
    if (pct <= 10) return TFT_RED;
    if (pct <= 25) return TFT_ORANGE;
    if (pct <= 50) return TFT_YELLOW;
    return TFT_GREEN;
}

static const char* batteryStatusLabel(int pct, uint32_t mv) {
    if (mv > 4350) return "BOOST/5V?";
    if (mv < 3000) return "CRITICAL";
    if (pct <= 10) return "LOW";
    if (pct <= 50) return "OK";
    if (pct < 95) return "GOOD";
    return "FULL";
}

static void readBattery(uint32_t& rawAvg, uint32_t& adcMvAvg, uint32_t& batMv) {
    uint32_t rawSum = 0;
    uint32_t mvSum = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
        rawSum += analogRead(VBAT_ADC_PIN);
        mvSum += analogReadMilliVolts(VBAT_ADC_PIN);
        delay(2);
    }
    rawAvg = rawSum / BAT_SAMPLES;
    adcMvAvg = mvSum / BAT_SAMPLES;
    batMv = (adcMvAvg * (BAT_R_TOP_OHMS + BAT_R_BOTTOM_OHMS)) / BAT_R_BOTTOM_OHMS;
}

static void drawBatteryBar(int x, int y, int w, int h, int pct, uint16_t color) {
    tft.drawRect(x, y, w, h, TFT_WHITE);
    tft.drawRect(x + w, y + h / 3, 4, h / 3, TFT_WHITE);
    tft.fillRect(x + 2, y + 2, w - 4, h - 4, TFT_BLACK);
    if (pct < 0) {
        for (int i = 0; i < w - 5; i += 8) tft.drawFastVLine(x + 3 + i, y + 3, h - 6, color);
        return;
    }
    int fillW = ((w - 4) * pct) / 100;
    tft.fillRect(x + 2, y + 2, fillW, h - 4, color);
}

void runBatteryStatus() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    drawToolFrame("BATTERY");
    beep(1800, 25);

    unsigned long lastDraw = 0;
    while (!isEnterPressed() && !isBackPressed()) {
        if (millis() - lastDraw > 500) {
            uint32_t raw = 0;
            uint32_t adcMv = 0;
            uint32_t batMv = 0;
            readBattery(raw, adcMv, batMv);

            bool looksBoostOut = batMv > 4350;
            int pct = looksBoostOut ? -1 : batteryPercentFromMv(batMv);
            uint16_t col = batteryColor(pct);

            tft.fillRect(10, 48, 300, 160, TFT_BLACK);
            drawStringCustom(12, 54, "GPIO " + String(VBAT_ADC_PIN) +
                "  DIV 2.2k/1k", TFT_CYAN, 1);
            drawStringCustom(12, 76, "Battery: " + String(batMv / 1000.0f, 2) + " V",
                col, 2);
            drawStringCustom(12, 106, "Level: " + String(pct < 0 ? "--" : String(pct)) + "%",
                col, 2);

            drawBatteryBar(142, 102, 126, 28, pct, col);

            drawStringCustom(12, 142, "Status: " +
                String(batteryStatusLabel(pct, batMv)), col, 1);
            drawStringCustom(12, 160, "ADC: " + String(adcMv) + " mV  raw:" + String(raw),
                TFT_WHITE, 1);

            if (looksBoostOut) {
                drawStringCustom(12, 184, "Estas midiendo salida 5V del step-up.", TFT_YELLOW, 1);
                drawStringCustom(12, 198, "Para porcentaje conecta ADC a la celda.", TFT_YELLOW, 1);
            } else {
                drawStringCustom(12, 184, "Estimacion para Li-ion/LiPo 1S.", TFT_WHITE, 1);
            }
            lastDraw = millis();
        }
        delay(10);
    }
    while (isEnterPressed() || isBackPressed()) delay(5);
}

static const char* wifiAuthName(uint8_t type) {
    switch (type) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "UNKNOWN";
    }
}

static String cleanReportField(const String& value) {
    String out = value;
    out.replace("\r", " ");
    out.replace("\n", " ");
    out.replace(",", " ");
    return out;
}

static String maskedBssid(const String& bssid) {
    if (bssid.length() < 17) return bssid;
    return bssid.substring(0, 8) + ":xx:xx:xx";
}

static void drawReportResult(const char* title, bool ok, const char* path) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, ok ? TFT_GREEN : TFT_RED);
    drawStringBig(10, 8, title, ok ? TFT_GREEN : TFT_RED, 1);
    tft.drawFastHLine(0, 34, 320, ok ? TFT_GREEN : TFT_RED);
    drawStringCustom(20, 82, ok ? "Archivo guardado:" : "No se pudo guardar", TFT_WHITE, 1);
    drawStringFit(20, 106, String(path), ok ? TFT_CYAN : TFT_YELLOW, 286, 2);
    if (!ok) drawStringCustom(20, 148, "Revisa SD/espacio/montaje.", TFT_YELLOW, 1);
    tft.drawFastHLine(0, 214, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK/BACK: RETURN", UI_ACCENT, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
}

static String gpsSummaryText() {
    drainGps(300);
    String out;
    out += "GPS chars: " + String(gps.charsProcessed()) + "\r\n";
    out += "Fix: " + String(gps.location.isValid() ? "YES" : "NO") + "\r\n";
    out += "Satellites: " +
           String(gps.satellites.isValid() ? String(gps.satellites.value()) : String("--")) + "\r\n";
    if (gps.location.isValid()) {
        out += "Lat: " + String(gps.location.lat(), 6) + "\r\n";
        out += "Lng: " + String(gps.location.lng(), 6) + "\r\n";
    }
    if (gps.hdop.isValid()) out += "HDOP: " + String(gps.hdop.hdop(), 1) + "\r\n";
    return out;
}

static void exportWifiAuditReport() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(10, 8, "WIFI AUDIT", TFT_WHITE, 1);
    tft.drawFastHLine(0, 34, 320, TFT_WHITE);
    drawStringCustom(24, 98, "Escaneando redes...", TFT_CYAN, 2);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(120);
    int n = WiFi.scanNetworks(false, true);

    String out;
    out.reserve(512 + max(n, 0) * 96);
    out += "CYBERDECK WIFI AUDIT\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += gpsSummaryText();
    out += "\r\nSSID,AUTH,CH,RSSI,BSSID_MASKED,FLAG\r\n";

    int openCount = 0;
    int weakCount = 0;
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            uint8_t auth = WiFi.encryptionType(i);
            String flag = "OK";
            if (auth == WIFI_AUTH_OPEN) {
                flag = "OPEN";
                openCount++;
            } else if (auth == WIFI_AUTH_WEP || auth == WIFI_AUTH_WPA_PSK) {
                flag = "WEAK";
                weakCount++;
            }
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) ssid = "<hidden>";
            out += cleanReportField(ssid) + ",";
            out += String(wifiAuthName(auth)) + ",";
            out += String(WiFi.channel(i)) + ",";
            out += String(WiFi.RSSI(i)) + ",";
            out += maskedBssid(WiFi.BSSIDstr(i)) + ",";
            out += flag + "\r\n";
        }
    }
    out += "\r\nSummary\r\n";
    out += "Networks: " + String(max(n, 0)) + "\r\n";
    out += "Open: " + String(openCount) + "\r\n";
    out += "Weak: " + String(weakCount) + "\r\n";
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);

    bool ok = sdWriteTextFile("/WIFI_AUDIT.csv", out);
    drawReportResult("WIFI AUDIT", ok, "/WIFI_AUDIT.csv");
}

static void exportGpsSnapshotReport() {
    beginGpsPort();
    String out;
    out += "CYBERDECK GPS SNAPSHOT\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += gpsSummaryText();
    if (gps.altitude.isValid()) out += "AltitudeM: " + String(gps.altitude.meters(), 1) + "\r\n";
    if (gps.speed.isValid()) out += "SpeedKmph: " + String(gps.speed.kmph(), 1) + "\r\n";
    if (gps.course.isValid()) out += "CourseDeg: " + String(gps.course.deg(), 1) + "\r\n";
    bool ok = sdWriteTextFile("/GPS_SNAPSHOT.txt", out);
    drawReportResult("GPS REPORT", ok, "/GPS_SNAPSHOT.txt");
}

static void exportBatteryReport() {
    uint32_t raw = 0;
    uint32_t adcMv = 0;
    uint32_t batMv = 0;
    readBattery(raw, adcMv, batMv);
    int pct = (batMv > 4350) ? -1 : batteryPercentFromMv(batMv);

    String out;
    out += "CYBERDECK BATTERY STATUS\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += "GPIO: " + String(VBAT_ADC_PIN) + "\r\n";
    out += "Divider: 2.2k/1k\r\n";
    out += "ADC_mV: " + String(adcMv) + "\r\n";
    out += "ADC_raw: " + String(raw) + "\r\n";
    out += "Battery_mV: " + String(batMv) + "\r\n";
    out += "Battery_V: " + String(batMv / 1000.0f, 2) + "\r\n";
    out += "Percent: " + String(pct < 0 ? String("--") : String(pct)) + "\r\n";
    out += "Status: " + String(batteryStatusLabel(pct, batMv)) + "\r\n";
    bool ok = sdWriteTextFile("/BATTERY_STATUS.txt", out);
    drawReportResult("BATTERY", ok, "/BATTERY_STATUS.txt");
}

void runAuditReports() {
    static const char* items[] = {
        "Export WiFi Audit",
        "Export GPS Snapshot",
        "Export Battery Status"
    };

    bool exitSub = false;
    while (!exitSub) {
        int choice = runSubMenu("AUDIT REPORTS", items, sizeof(items) / sizeof(char*));
        switch (choice) {
            case -1: exitSub = true;          break;
            case  0: exportWifiAuditReport(); break;
            case  1: exportGpsSnapshotReport(); break;
            case  2: exportBatteryReport();  break;
        }
    }
}

void runMissionDashboard() {
    while (isEnterPressed() || isBackPressed()) delay(5);
    beginGpsPort();
    beep(1800, 25);

    unsigned long lastDraw = 0;
    while (!isEnterPressed() && !isBackPressed()) {
        drainGps(60);
        if (millis() - lastDraw > 600) {
            uint32_t raw = 0, adcMv = 0, batMv = 0;
            readBattery(raw, adcMv, batMv);
            int pct = (batMv > 4350) ? -1 : batteryPercentFromMv(batMv);
            bool sdOk = beginSd();
            bool gpsFix = gps.location.isValid();
            int sats = gps.satellites.isValid() ? gps.satellites.value() : 0;

            tft.fillScreen(TFT_BLACK);
            tft.drawRect(0, 0, 320, 240, TFT_WHITE);
            drawStringBig(10, 8, "MISSION DASH", TFT_WHITE, 1);
            drawStringCustom(222, 12, "REAL STATUS", TFT_CYAN, 1);
            tft.drawFastHLine(0, 34, 320, TFT_WHITE);

            drawStringCustom(12, 48, "BAT", TFT_CYAN, 1);
            drawStringCustom(58, 44, String(batMv / 1000.0f, 2) + "V", batteryColor(pct), 2);
            drawBatteryBar(168, 44, 96, 18, pct, batteryColor(pct));
            drawStringCustom(272, 48, pct < 0 ? "--%" : String(pct) + "%", batteryColor(pct), 1);

            drawStringCustom(12, 82, "GPS", TFT_CYAN, 1);
            drawStringCustom(58, 78, gpsFix ? "FIX" : "NO FIX", gpsFix ? TFT_GREEN : TFT_YELLOW, 2);
            drawStringCustom(168, 82, "SAT:" + String(sats), TFT_WHITE, 1);
            drawStringCustom(228, 82, "NMEA:" + String(gps.charsProcessed()), TFT_WHITE, 1);

            drawStringCustom(12, 116, "SD", TFT_CYAN, 1);
            drawStringCustom(58, 112, sdOk ? "MOUNTED" : "ERROR", sdOk ? TFT_GREEN : TFT_RED, 2);
            if (sdOk) {
                uint64_t usedMb = SD.usedBytes() / (1024ULL * 1024ULL);
                uint64_t sizeMb = SD.cardSize() / (1024ULL * 1024ULL);
                drawStringCustom(168, 116, String((unsigned long)usedMb) + "/" +
                    String((unsigned long)sizeMb) + "MB", TFT_WHITE, 1);
            }

            drawStringCustom(12, 150, "NRF", TFT_CYAN, 1);
            drawStringCustom(58, 146, "Pins " + String(NRF1_CE_PIN) + "/" + String(NRF1_CSN_PIN) +
                " " + String(NRF2_CE_PIN) + "/" + String(NRF2_CSN_PIN), TFT_WHITE, 1);
            drawStringCustom(58, 164, "SPI " + String(SCK_PIN) + "/" + String(MISO_PIN) +
                "/" + String(MOSI_PIN), TFT_WHITE, 1);

            tft.drawFastHLine(0, 214, 320, UI_ACCENT);
            drawStringCustom(10, 222, "OK/BACK: RETURN", UI_ACCENT, 1);
            lastDraw = millis();
        }
        delay(10);
    }
    while (isEnterPressed() || isBackPressed()) delay(5);
}
