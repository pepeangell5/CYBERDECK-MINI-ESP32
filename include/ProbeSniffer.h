#ifndef PROBE_SNIFFER_H
#define PROBE_SNIFFER_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  PROBE REQUEST SNIFFER · captura los SSIDs que buscan celulares cercanos
//  · Modo promiscuo, filtra solo probe requests (subtype 0x04)
//  · Deduplica SSIDs y cuenta cuántas veces se vio cada uno
//  · Channel hopping (1 → 6 → 11) cada 2 segundos
//  · Lista navegable con SSID + count + último RSSI + último visto
// ═══════════════════════════════════════════════════════════════════════════

void runProbeSniffer();

// API para que KARMA Attack reuse la lista capturada (siguiente feature)
struct ProbeEntry {
    char     ssid[33];
    uint16_t count;
    uint16_t clients;
    int8_t   rssi;
    uint8_t  firstChannel;
    uint8_t  lastChannel;
    uint8_t  lastMac[6];
    uint32_t lastSeenMs;
};

int  probeSnifferGetCount();
bool probeSnifferGet(int idx, ProbeEntry& out);

#endif
