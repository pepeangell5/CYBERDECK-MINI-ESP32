#ifndef ARKANOID_H
#define ARKANOID_H

#include <Arduino.h>
#include "display_config.h" // O el nombre de tu archivo de configuración de pantalla

void launchArkanoid(LGFX &tft, bool &playing, int btnBack);

#endif