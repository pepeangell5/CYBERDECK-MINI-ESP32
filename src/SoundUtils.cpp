#include <Arduino.h>
#include "Settings.h"
#include "Pins.h"

void beep(int freq, int duration) {

#if BUZZER_PIN < 0
    (void)freq;
    (void)duration;
    return;
#else
    if (!soundEnabled) return;

    int duty = map(soundVolume, 1, 5, 50, 255);

    ledcWrite(0, duty);
    ledcWriteTone(0, freq);
    delay(duration);
    ledcWriteTone(0, 0);
#endif
}
