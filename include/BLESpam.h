#ifndef BLE_SPAM_H
#define BLE_SPAM_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  BLE SPAM · transmite advertisements BLE falsos
//  · 4 protocolos: Apple Continuity, Samsung, Microsoft Swift Pair, Google
//  · CHAOS MODE: rota entre los 4 aleatoriamente
//  · Propósito educativo/demo — usar con responsabilidad
// ═══════════════════════════════════════════════════════════════════════════

void runBLESpam();

#endif