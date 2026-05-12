#include "SplashScreen.h"

#include <Arduino.h>
#include <esp_system.h>

#include "AjoloteSprite.h"
#include "DisplayTFT.h"
#include "Input.h"
#include "NVSStore.h"
#include "PepeDraw.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

static constexpr uint16_t BOOT_BG     = TFT_BLACK;
static constexpr uint16_t BOOT_FG     = TFT_GREEN;
static constexpr uint16_t BOOT_DIM    = 0x0320;
static constexpr uint16_t BOOT_TEXT   = TFT_WHITE;
static constexpr uint16_t BOOT_MUTED  = 0x8410;
static constexpr uint16_t BOOT_ACCENT = TFT_CYAN;
static constexpr uint16_t BOOT_WARN   = TFT_YELLOW;

static void matrixRain(uint32_t durationMs) {
    const int cols = 32;
    int colY[cols];

    for (int i = 0; i < cols; i++) {
        colY[i] = -random(0, 240);
    }

    uint32_t start = millis();
    while (millis() - start < durationMs) {
        for (int c = 0; c < cols; c++) {
            int x = c * 10;
            int y = colY[c];
            if (y >= 0 && y < 240) {
                char ch = (char)random(33, 126);
                drawStringCustom(x, y, String(ch), BOOT_DIM, 1);
                if (y > 12) tft.fillRect(x, y - 12, 8, 10, BOOT_BG);
            }

            colY[c] += 4;
            if (colY[c] > 240) colY[c] = -random(0, 60);
        }
        delay(28);
    }

    tft.fillScreen(BOOT_BG);
}

static void revealAjolote(int x0, int y0) {
    for (int row = 0; row < AJOLOTE_HEIGHT; row++) {
        drawAjoloteRow(x0, y0, row, BOOT_FG);
        tft.drawFastHLine(x0, y0 + row, AJOLOTE_WIDTH, BOOT_ACCENT);
        delay(8);
        tft.drawFastHLine(x0, y0 + row, AJOLOTE_WIDTH, BOOT_BG);
        drawAjoloteRow(x0, y0, row, BOOT_FG);
    }
}

static void drawCentered(const String& text, int y, uint16_t color,
                         int size, FontType font) {
    int w = getTextWidth(text, size, font);
    int x = (320 - w) / 2;
    if (x < 0) x = 0;

    if (font == FONT_BIG) drawStringBig(x, y, text, color, size);
    else drawStringCustom(x, y, text, color, size);
}

static void typeLine(int x, int y, const char* prefix, const String& text,
                     uint16_t color, int charDelay = 12) {
    drawStringCustom(x, y, prefix, BOOT_MUTED, 1);
    int cursorX = x + getTextWidth(prefix, 1, FONT_SMALL);

    String partial;
    for (int i = 0; i < (int)text.length(); i++) {
        partial += text[i];
        tft.fillRect(cursorX, y, 320 - cursorX - 4, 10, BOOT_BG);
        drawStringCustom(cursorX, y, partial, color, 1);
        delay(charDelay);
    }
}

static void progressBar(int x, int y, int w, int h, uint32_t durationMs) {
    tft.drawRect(x, y, w, h, BOOT_FG);

    uint32_t start = millis();
    while (millis() - start < durationMs) {
        int fillW = ((millis() - start) * (w - 4)) / durationMs;
        if (fillW < 0) fillW = 0;
        if (fillW > w - 4) fillW = w - 4;
        tft.fillRect(x + 2, y + 2, fillW, h - 4, BOOT_FG);
        delay(20);
    }

    tft.fillRect(x + 2, y + 2, w - 4, h - 4, BOOT_FG);
}

static void waitForAnyInput(int x, int y) {
    while (isEnterPressed() || isBackPressed()) {
        delay(5);
    }

    bool visible = true;
    uint32_t lastBlink = millis();

    while (true) {
        NavAction action = readNavAction(40);
        if (action != NAV_NONE) {
            beep(2200, 50);
            delay(30);
            beep(2800, 60);
            break;
        }

        if (millis() - lastBlink > 480) {
            lastBlink = millis();
            visible = !visible;
            tft.fillRect(x - 4, y - 2, 140, 12, BOOT_BG);
            if (visible) drawStringCustom(x, y, "PRESS ANY KEY", BOOT_ACCENT, 1);
        }
        delay(20);
    }

    while (isEnterPressed() || isBackPressed()) {
        delay(5);
    }
}

void runSplashScreen() {
    tft.fillScreen(BOOT_BG);

    matrixRain(900);

    tft.drawRect(0, 0, 320, 240, BOOT_FG);
    tft.drawRect(2, 2, 316, 236, BOOT_DIM);

    int ajoX = (320 - AJOLOTE_WIDTH) / 2;
    int ajoY = 18;
    revealAjolote(ajoX, ajoY);
    beep(1500, 60);

    delay(120);
    int titleY = ajoY + AJOLOTE_HEIGHT + 6;
    int progressY = titleY + getFontHeight(2, FONT_BIG) + 4;
    int subtitleY = progressY + 13;

    drawCentered("CYBERDECK", titleY, BOOT_TEXT, 2, FONT_BIG);
    progressBar(70, progressY, 180, 8, 520);
    drawCentered("// ESP32-TOOLS PRO", subtitleY, BOOT_MUTED, 1, FONT_SMALL);
    beep(2200, 40);

    int logY = 164;
    typeLine(8, logY, "[OK] ", "ESP32-S3 @ " + String(ESP.getCpuFreqMHz()) + " MHz", BOOT_FG);
    logY += 12;
    typeLine(8, logY, "[OK] ", "TFT ST7789 240x320", BOOT_FG);
    logY += 12;
    typeLine(8, logY, "[OK] ", "GPS UART1 RX" + String(GPS_RX_PIN) + "/TX" + String(GPS_TX_PIN), BOOT_FG);
    logY += 12;
    typeLine(8, logY, "[OK] ", "SD SPI CS" + String(SD_CS_PIN), BOOT_WARN);

    unsigned long bootCount = nvsGetULong("boot_cnt", 0);
    drawStringCustom(8, 216, "BOOT #" + String(bootCount), BOOT_MUTED, 1);

    beep(2600, 50);
    delay(40);
    beep(3200, 70);

    int pressW = getTextWidth("PRESS ANY KEY", 1, FONT_SMALL);
    waitForAnyInput((320 - pressW) / 2, 226);

    tft.fillScreen(TFT_BLACK);
    delay(80);
}
