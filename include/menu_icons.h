#ifndef MENU_ICONS_H
#define MENU_ICONS_H

#include <Arduino.h>
#include "display_config.h" 

// Función para dibujar el glifo según el índice del menú
void drawGlifoIcon(LGFX &tft, int cx, int cy, int index);

#endif