#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  SPLASH SCREEN · pantalla de bienvenida al arrancar el dispositivo
//
//  Muestra:
//    · Ajolote pixel-art con lentes (mascota del proyecto)
//    · Título "ESP32-TOOLS" con animación type-on
//    · Autor "BY: PepeAngell"
//    · Secuencia de "loading steps" con beeps ascendentes
//    · Mensaje "PRESS OK" parpadeante al final
//    · Espera hasta que el usuario presione OK para continuar
// ═══════════════════════════════════════════════════════════════════════════

void runSplashScreen();

#endif