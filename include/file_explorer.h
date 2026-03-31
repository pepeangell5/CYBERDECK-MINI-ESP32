#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include <Arduino.h>
#include "display_config.h"


void showFileExplorer(LGFX &tft, bool &inSubMenu, int btnBack, int btnUp, int btnDown);

#endif