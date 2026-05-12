#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayTFT : public TFT_eSPI {
public:
    static constexpr int32_t LOGICAL_W = 320;
    static constexpr int32_t LOGICAL_H = 240;
    static constexpr int32_t SCALE_NUM = 1;
    static constexpr int32_t SCALE_DEN = 1;

    DisplayTFT() : TFT_eSPI() {}

    int32_t logicalToPhysicalX(int32_t x) {
        return mapX(x);
    }

    int32_t logicalToPhysicalY(int32_t y) {
        return mapY(y);
    }

    void drawNativeString(int32_t x, int32_t y, const String& text,
                          uint32_t color, uint32_t bg,
                          uint8_t font = 2, uint8_t size = 1) {
        TFT_eSPI::setTextColor(color, bg);
        TFT_eSPI::setTextSize(size);
        TFT_eSPI::setTextDatum(TL_DATUM);
        physicalMode = true;
        TFT_eSPI::drawString(text, mapX(x), mapY(y), font);
        physicalMode = false;
    }

    void drawNativeStringTransparent(int32_t x, int32_t y, const String& text,
                                     uint32_t color,
                                     uint8_t font = 2, uint8_t size = 1) {
        TFT_eSPI::setTextColor(color);
        TFT_eSPI::setTextSize(size);
        TFT_eSPI::setTextDatum(TL_DATUM);
        physicalMode = true;
        TFT_eSPI::drawString(text, mapX(x), mapY(y), font);
        physicalMode = false;
    }

    int32_t nativeTextWidth(const String& text, uint8_t font = 2, uint8_t size = 1) {
        TFT_eSPI::setTextSize(size);
        return (TFT_eSPI::textWidth(text, font) * SCALE_DEN) / SCALE_NUM;
    }

    void drawPixel(int32_t x, int32_t y, uint32_t color) override {
        if (physicalMode) {
            fillPhysicalRect(x, y, 1, 1, color);
            return;
        }
        fillPhysicalRect(mapX(x), mapY(y), scaledSpan(x, 1), scaledSpan(y, 1), color);
    }

    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) override {
        if (physicalMode) {
            TFT_eSPI::drawLine(x0, y0, x1, y1, color);
            return;
        }
        if (x0 == x1) {
            int32_t y = min(y0, y1);
            drawFastVLine(x0, y, abs(y1 - y0) + 1, color);
            return;
        }
        if (y0 == y1) {
            int32_t x = min(x0, x1);
            drawFastHLine(x, y0, abs(x1 - x0) + 1, color);
            return;
        }

        int32_t dx = abs(x1 - x0);
        int32_t sx = x0 < x1 ? 1 : -1;
        int32_t dy = -abs(y1 - y0);
        int32_t sy = y0 < y1 ? 1 : -1;
        int32_t err = dx + dy;

        while (true) {
            drawPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) return;
            int32_t e2 = err * 2;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) override {
        if (physicalMode) {
            fillPhysicalRect(x, y, 1, h, color);
            return;
        }
        fillRect(x, y, 1, h, color);
    }

    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) override {
        if (physicalMode) {
            fillPhysicalRect(x, y, w, 1, color);
            return;
        }
        fillRect(x, y, w, 1, color);
    }

    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) override {
        if (w <= 0 || h <= 0) return;
        if (physicalMode) {
            fillPhysicalRect(x, y, w, h, color);
            return;
        }
        fillPhysicalRect(mapX(x), mapY(y), scaledSpan(x, w), scaledSpan(y, h), color);
    }

    void fillScreen(uint32_t color) {
        TFT_eSPI::fillRect(0, 0, TFT_eSPI::width(), TFT_eSPI::height(), color);
    }

    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
        if (w <= 0 || h <= 0) return;
        drawFastHLine(x, y, w, color);
        drawFastHLine(x, y + h - 1, w, color);
        drawFastVLine(x, y, h, color);
        drawFastVLine(x + w - 1, y, h, color);
    }

    void drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
        if (r < 0) return;
        int32_t f = 1 - r;
        int32_t ddF_x = 1;
        int32_t ddF_y = -2 * r;
        int32_t x = 0;
        int32_t y = r;

        drawPixel(x0, y0 + r, color);
        drawPixel(x0, y0 - r, color);
        drawPixel(x0 + r, y0, color);
        drawPixel(x0 - r, y0, color);

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;

            drawPixel(x0 + x, y0 + y, color);
            drawPixel(x0 - x, y0 + y, color);
            drawPixel(x0 + x, y0 - y, color);
            drawPixel(x0 - x, y0 - y, color);
            drawPixel(x0 + y, y0 + x, color);
            drawPixel(x0 - y, y0 + x, color);
            drawPixel(x0 + y, y0 - x, color);
            drawPixel(x0 - y, y0 - x, color);
        }
    }

    void fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
        if (r < 0) return;
        drawFastVLine(x0, y0 - r, 2 * r + 1, color);

        int32_t f = 1 - r;
        int32_t ddF_x = 1;
        int32_t ddF_y = -2 * r;
        int32_t x = 0;
        int32_t y = r;

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;

            drawFastVLine(x0 + x, y0 - y, 2 * y + 1, color);
            drawFastVLine(x0 - x, y0 - y, 2 * y + 1, color);
            drawFastVLine(x0 + y, y0 - x, 2 * x + 1, color);
            drawFastVLine(x0 - y, y0 - x, 2 * x + 1, color);
        }
    }

private:
    bool physicalMode = false;

    static int32_t scaledValue(int32_t value) {
        int32_t numerator = value * SCALE_NUM;
        if (numerator >= 0) return numerator / SCALE_DEN;
        return -((-numerator + SCALE_DEN - 1) / SCALE_DEN);
    }

    int32_t viewportW() {
        return scaledValue(LOGICAL_W);
    }

    int32_t viewportH() {
        return scaledValue(LOGICAL_H);
    }

    int32_t originX() {
        return (TFT_eSPI::width() - viewportW()) / 2;
    }

    int32_t originY() {
        return (TFT_eSPI::height() - viewportH()) / 2;
    }

    int32_t mapX(int32_t x) {
        return originX() + scaledValue(x);
    }

    int32_t mapY(int32_t y) {
        return originY() + scaledValue(y);
    }

    static int32_t scaledSpan(int32_t start, int32_t span) {
        int32_t result = scaledValue(start + span) - scaledValue(start);
        return max<int32_t>(1, result);
    }

    void fillPhysicalRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
        TFT_eSPI::fillRect(x, y, w, h, color);
    }
};

extern DisplayTFT tft;
