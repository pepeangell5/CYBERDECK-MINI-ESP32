#ifndef AJOLOTE_SPRITE_H
#define AJOLOTE_SPRITE_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  AJOLOTE SPRITE · mascota oficial del firmware
//  · Bitmap monocromático 96x80 píxeles (960 bytes en PROGMEM)
//  · Compartido entre SplashScreen, About y Screensaver
// ═══════════════════════════════════════════════════════════════════════════

#define AJOLOTE_WIDTH   96
#define AJOLOTE_HEIGHT  80
#define AJOLOTE_BYTES   960

// Bitmap exportado (definido en AjoloteSprite.cpp)
extern const uint8_t AJOLOTE_BMP[AJOLOTE_BYTES];

// Dibuja una sola fila del ajolote en (x0, y0+row) usando el color dado.
// Usado por la animación scan-in del splash.
void drawAjoloteRow(int x0, int y0, int row, uint16_t color);

// Dibuja el ajolote completo en tamaño original (96x80) en (x0, y0).
void drawAjoloteFull(int x0, int y0, uint16_t color);

// Dibuja el ajolote escalado por un factor entero (1=normal, 2=doble, etc).
// Para escalas menores a 1.0, usa drawAjoloteHalf (más eficiente).
void drawAjoloteScaled(int x0, int y0, uint16_t color, int scale);

// Dibuja el ajolote a la mitad del tamaño (48x40) — versión optimizada
// que toma 1 de cada 2 píxeles en cada eje.
void drawAjoloteHalf(int x0, int y0, uint16_t color);

#endif