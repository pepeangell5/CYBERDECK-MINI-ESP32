#ifndef GAMES_MENU_H
#define GAMES_MENU_H

#include <Arduino.h>
#include "display_config.h"
#include "games/snake.h"

void showGamesMenu(LGFX &tft, bool &inSubMenu, int btnBack);

#endif