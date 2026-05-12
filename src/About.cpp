#include "About.h"
#include "DisplayTFT.h"
#include "PepeDraw.h"
#include "Pins.h"
#include "SoundUtils.h"
#include "NVSStore.h"
#include "SystemInfo.h"
#include "AjoloteSprite.h"

extern DisplayTFT tft;

// ═══════════════════════════════════════════════════════════════════════════
//  CONFIG
// ═══════════════════════════════════════════════════════════════════════════
#define VIEWPORT_TOP    34     // donde empieza el viewport (debajo del header)
#define VIEWPORT_BOTTOM 218    // donde termina (arriba del footer)
#define SCROLL_STEP     20

static int g_scrollY = 0;
static int g_maxScroll = 0;

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS DE DIBUJO CON CLIPPING (no dibuja fuera del viewport)
// ═══════════════════════════════════════════════════════════════════════════

// Dibuja texto SOLO si cae completamente dentro del viewport.
// Si está parcialmente fuera, no lo dibuja (evita manchar header/footer).
static void drawScrollableText(int yContent, int x, const String& text,
                                uint16_t color, int size) {
    int yScreen = VIEWPORT_TOP + (yContent - g_scrollY);
    int textH = (size == 1) ? 7 : (size * 8);

    // Si está completamente fuera del viewport, no dibujar
    if (yScreen + textH < VIEWPORT_TOP) return;
    if (yScreen > VIEWPORT_BOTTOM) return;

    // Si está parcialmente fuera, tampoco — evita que se desborde
    if (yScreen < VIEWPORT_TOP) return;
    if (yScreen + textH > VIEWPORT_BOTTOM) return;

    drawStringCustom(x, yScreen, text, color, size);
}

static void drawScrollableLine(int yContent, uint16_t color) {
    int yScreen = VIEWPORT_TOP + (yContent - g_scrollY);
    if (yScreen < VIEWPORT_TOP || yScreen > VIEWPORT_BOTTOM) return;
    tft.drawFastHLine(15, yScreen, 290, color);
}

// Ajolote a media escala (48x40). Recorta filas individuales si quedan
// parcialmente fuera del viewport.
static void drawScrollableAjolote(int yContent) {
    const int W = 48;
    const int H = 40;
    int x = (320 - W) / 2;
    int yBase = VIEWPORT_TOP + (yContent - g_scrollY);

    // Si está completamente fuera, salir
    if (yBase + H < VIEWPORT_TOP) return;
    if (yBase > VIEWPORT_BOTTOM) return;

    // Dibujar fila por fila, saltando las que estén fuera del viewport
    int bytesPerRow = AJOLOTE_WIDTH / 8;
    for (int r = 0; r < AJOLOTE_HEIGHT; r += 2) {
        int outY = yBase + r / 2;
        if (outY < VIEWPORT_TOP) continue;     // arriba del viewport
        if (outY > VIEWPORT_BOTTOM) break;     // ya pasamos el viewport

        for (int byteIdx = 0; byteIdx < bytesPerRow; byteIdx++) {
            uint8_t bits = pgm_read_byte(
                &AJOLOTE_BMP[r * bytesPerRow + byteIdx]);
            if (bits == 0) continue;
            for (int bit = 0; bit < 8; bit += 2) {
                if (bits & (0x80 >> bit)) {
                    int outX = x + (byteIdx * 8 + bit) / 2;
                    tft.drawPixel(outX, outY, UI_MAIN);
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CONTENIDO PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════════

static void drawAboutContent() {
    // Limpiar el viewport (NO el header ni el footer)
    tft.fillRect(2, VIEWPORT_TOP, 316, VIEWPORT_BOTTOM - VIEWPORT_TOP,
                 TFT_BLACK);

    int y = 5;   // posición Y dentro del contenido virtual

    // ─── Título grande ───
    String title = "ESP32-TOOLS";
    int tw = title.length() * 8 * 3;   // size 3 con FONT_BIG
    drawScrollableText(y, (320 - tw) / 2, title, UI_MAIN, 3);
    y += 32;

    // ─── Versión ───
    String version = String(FW_VERSION);
    int vw = version.length() * 6 * 2;
    drawScrollableText(y, (320 - vw) / 2, version, UI_SELECT, 2);
    y += 28;

    drawScrollableLine(y, UI_ACCENT);
    y += 12;

    // ─── Ajolote (48x40) ───
    drawScrollableAjolote(y);
    y += 50;

    drawScrollableLine(y, UI_ACCENT);
    y += 14;

    // ─── Autor ───
    drawScrollableText(y, 70, "By PepeAngell", TFT_YELLOW, 2);
    y += 28;

    drawScrollableText(y, 30, "Jose Angel", UI_MAIN, 2);
    y += 22;
    drawScrollableText(y, 30, "Chavez Felix", UI_MAIN, 2);
    y += 28;

    drawScrollableText(y, 30, "Los Mochis, Sinaloa", UI_ACCENT, 1);
    y += 12;
    drawScrollableText(y, 30, "Mexico", UI_ACCENT, 1);
    y += 18;

    drawScrollableLine(y, UI_ACCENT);
    y += 14;

    // ─── Redes sociales ───
    drawScrollableText(y, 30, "REDES SOCIALES", UI_MAIN, 1);
    y += 20;

    drawScrollableText(y, 30, "IG:", TFT_CYAN, 2);
    drawScrollableText(y, 80, "@pepeangelll", UI_MAIN, 2);
    y += 26;

    drawScrollableText(y, 30, "FB:", 0x041F, 2);
    drawScrollableText(y, 80, "/esp32tools", UI_MAIN, 2);
    y += 26;

    drawScrollableText(y, 30, "GH:", 0xA81F, 2);
    drawScrollableText(y, 80, "/pepeangell5", UI_MAIN, 2);
    y += 30;

    drawScrollableLine(y, UI_ACCENT);
    y += 14;

    // ─── Boot count ───
    int boots = nvsGetInt("boot_cnt", 0);
    String bootText = "Booteado " + String(boots) + " veces";
    int bw = bootText.length() * 6;
    drawScrollableText(y, (320 - bw) / 2, bootText, UI_ACCENT, 1);
    y += 18;

    drawScrollableLine(y, UI_ACCENT);
    y += 14;

    // ─── Quote / filosofía ───
    drawScrollableText(y, 30, "\"El conocimiento", TFT_GREEN, 2);
    y += 22;
    drawScrollableText(y, 30, "debe ser libre.\"", TFT_GREEN, 2);
    y += 32;

    drawScrollableText(y, 80, "HECHO", UI_ACCENT, 1);
    y += 12;
    drawScrollableText(y, 100, "EN MÉXICO", UI_ACCENT, 1);
    y += 25;

    // Calcular max scroll
    int viewportH = VIEWPORT_BOTTOM - VIEWPORT_TOP;
    g_maxScroll = y - viewportH;
    if (g_maxScroll < 0) g_maxScroll = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//  HEADER Y FOOTER (se redibujan SIEMPRE encima para evitar manchas)
// ═══════════════════════════════════════════════════════════════════════════

static void drawHeader() {
    tft.fillRect(0, 0, 320, VIEWPORT_TOP, TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringCustom(110, 10, "ABOUT", UI_MAIN, 3);
    tft.drawFastHLine(2, VIEWPORT_TOP, 316, UI_ACCENT);
}

static void drawFooter() {
    tft.fillRect(0, VIEWPORT_BOTTOM, 320, 240 - VIEWPORT_BOTTOM, TFT_BLACK);
    tft.drawFastHLine(2, VIEWPORT_BOTTOM, 316, UI_ACCENT);

    // Re-dibujar bordes laterales por si se mancharon
    tft.drawFastVLine(0, 0, 240, UI_MAIN);
    tft.drawFastVLine(319, 0, 240, UI_MAIN);
    tft.drawFastHLine(0, 239, 320, UI_MAIN);

    if (g_maxScroll > 0) {
        if (g_scrollY == 0) {
            drawStringCustom(10, 226, "DOWN: VER MAS  OK: VOLVER",
                             UI_ACCENT, 1);
        } else if (g_scrollY >= g_maxScroll) {
            drawStringCustom(10, 226, "UP: SUBIR  OK: VOLVER",
                             UI_ACCENT, 1);
        } else {
            drawStringCustom(10, 226, "UP/DN: SCROLL  OK: VOLVER",
                             UI_ACCENT, 1);
        }

        // Indicador de scroll lateral
        int trackTop = VIEWPORT_TOP + 5;
        int trackBot = VIEWPORT_BOTTOM - 5;
        int trackH = trackBot - trackTop;
        int totalContent = g_maxScroll + (VIEWPORT_BOTTOM - VIEWPORT_TOP);
        int barH = (trackH * (VIEWPORT_BOTTOM - VIEWPORT_TOP)) / totalContent;
        if (barH < 10) barH = 10;
        int barY = trackTop;
        if (g_maxScroll > 0) {
            barY = trackTop + (g_scrollY * (trackH - barH)) / g_maxScroll;
        }
        // Limpiar track antes
        tft.fillRect(310, trackTop, 6, trackH, TFT_BLACK);
        tft.drawFastVLine(312, trackTop, trackH, UI_ACCENT);
        tft.fillRect(310, barY, 5, barH, UI_SELECT);
    } else {
        drawStringCustom(110, 226, "OK: VOLVER", UI_ACCENT, 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  REDIBUJADO COMPLETO (orden importante: contenido → header → footer)
// ═══════════════════════════════════════════════════════════════════════════

static void redrawAll() {
    drawAboutContent();    // 1. contenido scrolleable (puede manchar bordes)
    drawHeader();          // 2. header encima → tapa cualquier mancha arriba
    drawFooter();          // 3. footer encima → tapa cualquier mancha abajo
}

// ═══════════════════════════════════════════════════════════════════════════
//  ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════

void runAbout() {
    while (navEnterPressed()) delay(5);
    delay(100);

    g_scrollY = 0;

    // Beep de entrada (jingle de credits)
    beep(2400, 60); delay(40);
    beep(3000, 60); delay(40);
    beep(3600, 100);

    tft.fillScreen(TFT_BLACK);
    redrawAll();

    unsigned long lastBtn = 0;

    while (true) {
        if (navEnterPressed() && millis() - lastBtn > 200) {
            beep(1800, 50); delay(30);
            beep(1200, 80);
            while (navEnterPressed()) delay(5);
            delay(100);
            return;
        }

        if (navUpPressed() && millis() - lastBtn > 150) {
            if (g_scrollY > 0) {
                g_scrollY -= SCROLL_STEP;
                if (g_scrollY < 0) g_scrollY = 0;
                beep(2200, 20);
                redrawAll();
            }
            lastBtn = millis();
        }

        if (navDownPressed() && millis() - lastBtn > 150) {
            if (g_scrollY < g_maxScroll) {
                g_scrollY += SCROLL_STEP;
                if (g_scrollY > g_maxScroll) g_scrollY = g_maxScroll;
                beep(2200, 20);
                redrawAll();
            }
            lastBtn = millis();
        }

        delay(15);
    }
}
