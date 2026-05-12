#ifndef ICONS_H
#define ICONS_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  ICONS · Bitmaps pixel-art para el menú principal
//  · Formato: 64x64 pixels monocromo
//  · Cada byte = 8 pixels horizontales (bit MSB = izquierda)
//  · 64 * 64 / 8 = 512 bytes por ícono
//  · Dibujables con color personalizado (blanco normal, naranja al seleccionar)
// ═══════════════════════════════════════════════════════════════════════════

#define ICON_SIZE       64
#define ICON_BYTES      (ICON_SIZE * ICON_SIZE / 8)   // 512 bytes

// ── IDs de íconos ─────────────────────────────────────────────────────────
enum IconID {
    ICON_WIFI = 0,
    ICON_RADIO,
    ICON_BLUETOOTH,
    ICON_MONITOR,
    ICON_SYSTEM,
    ICON_COUNT
};

// Dibuja un ícono centrado en (cx, cy) con el color dado.
// Los pixels fuera del rango vertical [yMin, yMax] se descartan (clipping).
// Usa drawIcon(..., 0, 239) si no necesitas clipping.
void drawIcon(int cx, int cy, IconID id, uint16_t color,
              int yMin = 0, int yMax = 239);

#endif