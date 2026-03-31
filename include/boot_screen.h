#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include <Arduino.h>
#include "display_config.h"

#define MATRIX_GREEN 0x07E0 

void drawBootScreen(LGFX &tft);

#endif