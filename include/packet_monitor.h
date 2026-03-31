#ifndef PACKET_MONITOR_H
#define PACKET_MONITOR_H

#include <Arduino.h>

class LGFX;

void showPacketMonitor(LGFX &tft, bool &inSubMenu, int btnBack);

#endif