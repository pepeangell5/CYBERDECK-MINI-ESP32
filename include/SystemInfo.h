#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  SYSTEM INFO · pantalla de diagnóstico del dispositivo
//  · Muestra firmware, hardware y runtime stats en vivo
//  · Auto-refresh cada 500ms (uptime, heap, temperatura)
//  · Salir con OK mantenido ~300ms
// ═══════════════════════════════════════════════════════════════════════════

// Versión del firmware (cambiar al lanzar releases nuevas)
#define FW_NAME     "ESP32-TOOLS"
#define FW_VERSION  "v1.0"

void runSystemInfo();

#endif