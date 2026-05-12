#ifndef NVS_STORE_H
#define NVS_STORE_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  NVSStore · wrapper simple sobre ESP32 Preferences (NVS)
//  · Persistencia en flash: sobrevive reinicios y apagones
//  · Claves limitadas a 15 caracteres (limitación de NVS)
//  · API minimalista: get con default, set, commit
// ═══════════════════════════════════════════════════════════════════════════

// Inicializa el namespace de NVS. Llamar UNA SOLA VEZ desde setup().
void nvsBegin();

// Cerrar (opcional, al final del programa)
void nvsEnd();

// ── Lectura ───────────────────────────────────────────────────────────────
//   Devuelven el valor guardado, o `defaultValue` si la clave no existe.
bool          nvsGetBool(const char* key, bool defaultValue);
int           nvsGetInt(const char* key, int defaultValue);
unsigned long nvsGetULong(const char* key, unsigned long defaultValue);
String        nvsGetString(const char* key, const String& defaultValue);

// ── Escritura ─────────────────────────────────────────────────────────────
//   Guardan inmediatamente en flash.
void nvsSetBool(const char* key, bool value);
void nvsSetInt(const char* key, int value);
void nvsSetULong(const char* key, unsigned long value);
void nvsSetString(const char* key, const String& value);

// ── Utilidades ────────────────────────────────────────────────────────────
void nvsErase(const char* key);      // borra una clave específica
void nvsEraseAll();                  // ⚠️ borra TODAS las preferencias

#endif