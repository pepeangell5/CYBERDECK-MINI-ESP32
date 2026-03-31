#ifndef WIFI_TOOL_H
#define WIFI_TOOL_H

#include <Arduino.h>
#include <WiFi.h>
#include "display_config.h"

void setupWiFi();
void showWiFiScanner(LGFX &tft, bool &inSubMenu, int btnBack, int btnEnter);

#endif