#ifndef EVIL_PORTAL_H
#define EVIL_PORTAL_H

#include <Arduino.h>

class LGFX; 

void showEvilPortal(LGFX &tft, bool &inSubMenu, int btnBack);
void showLogs(LGFX &tft, int btnBack);
void deleteLogs(LGFX &tft); 

#endif