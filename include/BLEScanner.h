#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  BLE SCANNER · descubre dispositivos Bluetooth Low Energy cercanos
//  · Scan continuo (hasta que usuario sale con OK-hold)
//  · Lista ordenada por RSSI (más cercanos arriba)
//  · Vendor lookup por OUI (Apple, Samsung, Xiaomi, etc.)
//  · Detalles por dispositivo: MAC, RSSI, servicios, manufacturer data
// ═══════════════════════════════════════════════════════════════════════════

void runBLEScanner();

#endif