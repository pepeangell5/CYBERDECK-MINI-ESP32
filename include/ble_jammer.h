#ifndef BLE_JAMMER_H
#define BLE_JAMMER_H

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
// Ajusta este include al nombre exacto de tu archivo de configuración de pantalla
#include "display_config.h" 

// Declaración de la función para que sea visible desde tu main.cpp o menu.cpp
void launchBLEJammer(LGFX &tft, bool &playing, int btnBack);

#endif