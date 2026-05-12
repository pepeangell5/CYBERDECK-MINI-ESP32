#include "Input.h"
#include "Pins.h"

static int lastEncClk = HIGH;
static unsigned long lastNavMs = 0;
static NavAction pendingEncoderAction = NAV_NONE;
static unsigned long lastEncoderMs = 0;
static bool upWasDown = false;
static bool downWasDown = false;
static bool enterWasDown = false;
static bool backWasDown = false;

static void pollEncoder() {
    int encClk = digitalRead(ENC_CLK_PIN);
    if (encClk != lastEncClk) {
        lastEncClk = encClk;
        if (encClk == LOW && millis() - lastEncoderMs > 4) {
            pendingEncoderAction = digitalRead(ENC_DT_PIN) == HIGH ? NAV_DOWN : NAV_UP;
            lastEncoderMs = millis();
        }
    }
}

static bool takePendingEncoderAction(NavAction action) {
    pollEncoder();
    if (pendingEncoderAction != action) return false;
    pendingEncoderAction = NAV_NONE;
    lastNavMs = millis();
    return true;
}

void initInput() {
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_OK, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
    pinMode(ENC_CLK_PIN, INPUT_PULLUP);
    pinMode(ENC_DT_PIN, INPUT_PULLUP);
    pinMode(ENC_SW_PIN, INPUT_PULLUP);
    lastEncClk = digitalRead(ENC_CLK_PIN);
    upWasDown = digitalRead(BTN_UP) == LOW;
    downWasDown = digitalRead(BTN_DOWN) == LOW;
    enterWasDown = isEnterPressed();
    backWasDown = digitalRead(BTN_BACK) == LOW;
}

void flushNavInput(unsigned long settleMs) {
    if (settleMs > 0) delay(settleMs);
    pendingEncoderAction = NAV_NONE;
    lastEncClk = digitalRead(ENC_CLK_PIN);
    upWasDown = digitalRead(BTN_UP) == LOW;
    downWasDown = digitalRead(BTN_DOWN) == LOW;
    enterWasDown = isEnterPressed();
    backWasDown = digitalRead(BTN_BACK) == LOW;
    lastNavMs = millis();
}

bool isBackPressed() {
    return digitalRead(BTN_BACK) == LOW;
}

bool isEnterPressed() {
    return digitalRead(BTN_OK) == LOW || digitalRead(ENC_SW_PIN) == LOW;
}

bool navUpPressed() {
    if (digitalRead(BTN_UP) == LOW) return true;
    return takePendingEncoderAction(NAV_UP);
}

bool navDownPressed() {
    if (digitalRead(BTN_DOWN) == LOW) return true;
    return takePendingEncoderAction(NAV_DOWN);
}

bool navEnterPressed() {
    return isEnterPressed();
}

bool navBackPressed() {
    return isBackPressed();
}

NavAction readNavAction(unsigned long repeatMs) {
    unsigned long now = millis();
    pollEncoder();

    bool backDown = digitalRead(BTN_BACK) == LOW;
    bool upDown = digitalRead(BTN_UP) == LOW;
    bool downDown = digitalRead(BTN_DOWN) == LOW;
    bool enterDown = isEnterPressed();

    NavAction action = NAV_NONE;
    bool canEmit = (now - lastNavMs) >= repeatMs;

    if (canEmit && backDown && !backWasDown) {
        action = NAV_BACK;
    } else if (canEmit && upDown && !upWasDown) {
        action = NAV_UP;
    } else if (canEmit && downDown && !downWasDown) {
        action = NAV_DOWN;
    } else if (canEmit && enterDown && !enterWasDown) {
        action = NAV_ENTER;
    } else if (canEmit && pendingEncoderAction != NAV_NONE) {
        action = pendingEncoderAction;
        pendingEncoderAction = NAV_NONE;
    }

    backWasDown = backDown;
    upWasDown = upDown;
    downWasDown = downDown;
    enterWasDown = enterDown;

    if (action != NAV_NONE) {
        lastNavMs = now;
    }
    return action;
}
