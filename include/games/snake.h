#ifndef SNAKE_H
#define SNAKE_H

#include <Arduino.h>
#include "display_config.h"

// Definimos la función principal del juego
// Recibe la pantalla, la bandera de submenú y el pin del botón para salir
void launchSnake(LGFX &tft, bool &inSubMenu, int btnBack);

#endif