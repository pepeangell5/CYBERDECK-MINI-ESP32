#ifndef BEACON_SPAM_H
#define BEACON_SPAM_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  BEACON SPAM · transmite redes WiFi falsas (frames 802.11 Beacon)
//  · 5 modos: Mexipicante, Memes, Paranoia, Chaos UTF-8, Mix Total
//  · Channel hopping automático (1 → 6 → 11)
//  · BSSID rotation por cada SSID
//  · Uso educativo/demo — usar con responsabilidad
// ═══════════════════════════════════════════════════════════════════════════

void runBeaconSpam();

#endif