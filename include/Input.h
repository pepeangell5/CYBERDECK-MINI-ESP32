#pragma once

#include <Arduino.h>

enum NavAction {
    NAV_NONE = 0,
    NAV_UP,
    NAV_DOWN,
    NAV_ENTER,
    NAV_BACK
};

void initInput();
void flushNavInput(unsigned long settleMs = 20);
NavAction readNavAction(unsigned long repeatMs = 140);
bool isBackPressed();
bool isEnterPressed();
bool navUpPressed();
bool navDownPressed();
bool navEnterPressed();
bool navBackPressed();
