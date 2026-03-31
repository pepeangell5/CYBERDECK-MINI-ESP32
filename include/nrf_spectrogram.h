#ifndef NRF_SPECTROGRAM_H
#define NRF_SPECTROGRAM_H

#include <Arduino.h>
#include "display_config.h"

// Función para el analizador de espectro visual
void showNRFSpectrogram(LGFX &tft, bool &inSubMenu, int btnBack);

#endif