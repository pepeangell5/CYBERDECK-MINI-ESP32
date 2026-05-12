#ifndef RADIOSCANNER_H
#define RADIOSCANNER_H

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include "PepeDraw.h"

// Prototipo de la función principal
void runRadioScanner();
void runRfBaseline();

#endif
