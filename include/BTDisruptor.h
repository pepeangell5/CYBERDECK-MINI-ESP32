#ifndef BT_DISRUPTOR_H
#define BT_DISRUPTOR_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  BT DISRUPTOR · ataque dirigido a un dispositivo BLE específico
//  · Scan → Select target → Select mode → Attack
//  · 4 modos: Connect Flood, L2CAP Ping Storm, Spoof Identity, Chaos
//  · Uso educativo/demo — usar con responsabilidad
// ═══════════════════════════════════════════════════════════════════════════

void runBTDisruptor();

#endif