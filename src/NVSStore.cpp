#include "NVSStore.h"
#include <Preferences.h>

// ═══════════════════════════════════════════════════════════════════════════
//  Namespace de NVS.
//  Todas las claves se guardan bajo este namespace para no colisionar
//  con otras librerías que usen Preferences.
// ═══════════════════════════════════════════════════════════════════════════
static const char* NVS_NAMESPACE = "esp32tools";

static Preferences prefs;
static bool initialized = false;

void nvsBegin() {
    if (initialized) return;
    // false = modo lectura/escritura
    prefs.begin(NVS_NAMESPACE, false);
    initialized = true;
}

void nvsEnd() {
    if (!initialized) return;
    prefs.end();
    initialized = false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  GETTERS
// ═══════════════════════════════════════════════════════════════════════════
bool nvsGetBool(const char* key, bool defaultValue) {
    if (!initialized) return defaultValue;
    return prefs.getBool(key, defaultValue);
}

int nvsGetInt(const char* key, int defaultValue) {
    if (!initialized) return defaultValue;
    return prefs.getInt(key, defaultValue);
}

unsigned long nvsGetULong(const char* key, unsigned long defaultValue) {
    if (!initialized) return defaultValue;
    return prefs.getULong(key, defaultValue);
}

String nvsGetString(const char* key, const String& defaultValue) {
    if (!initialized) return defaultValue;
    return prefs.getString(key, defaultValue);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SETTERS
// ═══════════════════════════════════════════════════════════════════════════
void nvsSetBool(const char* key, bool value) {
    if (!initialized) return;
    prefs.putBool(key, value);
}

void nvsSetInt(const char* key, int value) {
    if (!initialized) return;
    prefs.putInt(key, value);
}

void nvsSetULong(const char* key, unsigned long value) {
    if (!initialized) return;
    prefs.putULong(key, value);
}

void nvsSetString(const char* key, const String& value) {
    if (!initialized) return;
    prefs.putString(key, value);
}

// ═══════════════════════════════════════════════════════════════════════════
//  ERASE
// ═══════════════════════════════════════════════════════════════════════════
void nvsErase(const char* key) {
    if (!initialized) return;
    prefs.remove(key);
}

void nvsEraseAll() {
    if (!initialized) return;
    prefs.clear();
}