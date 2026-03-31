#ifndef GPS_TOOL_H
#define GPS_TOOL_H

#include <Arduino.h>

class LGFX;

void showGPSTool(LGFX &tft, bool &inSubMenu, int btnBack);

#endif