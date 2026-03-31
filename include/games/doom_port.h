#ifndef DOOM_PORT_H
#define DOOM_PORT_H

#include <Arduino.h>
#include "display_config.h"

void launchDoom(LGFX &tft, bool &playing, int btnBack);

#endif