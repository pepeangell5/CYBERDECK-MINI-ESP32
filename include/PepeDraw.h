#ifndef PEPE_DRAW_H
#define PEPE_DRAW_H

#include <Arduino.h>
#include "DisplayTFT.h"

// ═══════════════════════════════════════════════════════════════════════════
//  PEPE-DRAW v2  ·  Librería de renderizado de texto bitmap
//  · 2 fuentes propias: SMALL (5x7) y BIG (8x12)
//  · Ancho variable por carácter (kerning real)
//  · Descenders reales (g j p q y)
//  · Soporte español: á é í ó ú ü ñ Ñ Á É Í Ó Ú Ü ¿ ¡
//  · UTF-8 aware
// ═══════════════════════════════════════════════════════════════════════════

// ── Colores de UI ─────────────────────────────────────────────────────────
#define UI_MAIN    TFT_WHITE
#define UI_BG      TFT_BLACK
#define UI_ACCENT  TFT_WHITE
#define UI_CURSOR  TFT_WHITE
#define UI_SELECT  TFT_WHITE

extern DisplayTFT tft;

// ── Tipos de fuente ───────────────────────────────────────────────────────
enum FontType {
    FONT_SMALL = 0,   // 5 ancho × 7 alto (compacta, legible)
    FONT_BIG   = 1    // 8 ancho × 12 alto (titulares, headers)
};

// ───────────────────────────────────────────────────────────────────────────
//  API compatible hacia atrás (no rompe código existente)
//  drawCharCustom / drawStringCustom → usan FONT_SMALL internamente
// ───────────────────────────────────────────────────────────────────────────
void drawCharCustom(int x, int y, char c, uint16_t color, int size);
void drawStringCustom(int x, int y, String txt, uint16_t color, int size);

// ───────────────────────────────────────────────────────────────────────────
//  API nueva
// ───────────────────────────────────────────────────────────────────────────

// Dibuja con fuente BIG (8x12)
void drawStringBig(int x, int y, const String& txt, uint16_t color, int size);

// Calcula el ancho en píxeles de un texto (útil para centrar o alinear)
int  getTextWidth(const String& txt, int size, FontType font = FONT_SMALL);

// Altura en píxeles de la fuente (para calcular posiciones verticales)
int  getFontHeight(int size, FontType font = FONT_SMALL);

// Recorta texto para que quepa dentro de maxWidth pixeles y agrega "..".
String truncateToWidth(const String& txt, int maxWidth,
                       int size, FontType font = FONT_SMALL);

// Dibuja texto recortado a un ancho maximo.
void drawStringFit(int x, int y, const String& txt, uint16_t color,
                   int maxWidth, int size, FontType font = FONT_SMALL);

// Dibuja centrado horizontalmente en la pantalla (ancho 320)
void drawStringCentered(int y, const String& txt, uint16_t color,
                        int size, FontType font = FONT_SMALL);

// Dibuja alineado a la derecha (xRight = borde derecho del texto)
void drawStringRight(int xRight, int y, const String& txt, uint16_t color,
                     int size, FontType font = FONT_SMALL);

#endif
