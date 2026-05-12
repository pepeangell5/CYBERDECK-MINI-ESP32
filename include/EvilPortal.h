#ifndef EVIL_PORTAL_H
#define EVIL_PORTAL_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  EVIL PORTAL · captura credenciales con AP falso + portal cautivo
//  · 10 SSIDs predefinidos (ISPs, cafeterías, públicos)
//  · Modo SIMPLE: AP fijo con SSID elegido
//  · Modo CLONE: clona red real + deauth simultáneo
//  · 4 plataformas de phishing (FB, Google, IG, TikTok)
//  · Logs persistentes en NVS, ver/borrar desde el menú
//  · USO EDUCATIVO/PENTESTING ÚNICAMENTE
// ═══════════════════════════════════════════════════════════════════════════

void runEvilPortal();

#endif