#ifndef BEACON_TOOL_H
#define BEACON_TOOL_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "display_config.h"

void showBeaconSpammer(LGFX &tft, bool &inSubMenu, int btnBack);

#endif