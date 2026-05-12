#ifndef KARMA_H
#define KARMA_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  KARMA ATTACK · responde con beacons a los probes capturados
//  · Reusa la lista del Probe Sniffer
//  · Anuncia cada SSID buscado como red abierta disponible
//  · Channel hopping 1 → 6 → 11
//  · Dispositivos con redes abiertas guardadas pueden auto-conectar
// ═══════════════════════════════════════════════════════════════════════════

void runKarma();

#endif