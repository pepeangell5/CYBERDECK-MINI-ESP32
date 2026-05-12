#ifndef SCREENSAVER_H
#define SCREENSAVER_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  SCREENSAVER · ajolote caminando estilo "DVD logo"
//  · Se activa desde el menú principal después de IDLE_TIMEOUT_MS sin input
//  · Sale con cualquier botón (UP/DOWN/OK)
//  · Estrellitas titilando + textos rotativos
// ═══════════════════════════════════════════════════════════════════════════

#define SCREENSAVER_IDLE_MS  30000   // 30 segundos

// Ejecuta el screensaver. Bloquea hasta que el usuario toque un botón.
// Cuando regresa, debe redibujarse el menú actual.
void runScreensaver();

#endif