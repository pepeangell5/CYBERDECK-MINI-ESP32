#include "PacketMonitor.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include "Settings.h"
#include "Pins.h"
#include "SoundUtils.h"

// ═════════════════════════════════════════════════════════════════════════════
//  CONFIGURACIÓN
// ═════════════════════════════════════════════════════════════════════════════
#define HISTORY_SIZE    60        // 60 segundos de historial
#define BAR_WIDTH       5         // px por cada barra

// Tope para escalar el meter y el history.
// Calibrado para entornos reales (ESP32 solo cuenta frames 802.11 válidos).
#define PPS_MAX_SCALE   500

// ── Layout ─────────────────────────────────────────────────────────────────
#define HISTORY_X     12
#define HISTORY_Y     140
#define HISTORY_W     300
#define HISTORY_H     45

#define METER_X       282
#define METER_Y       30
#define METER_W       28
#define METER_H       100

// ═════════════════════════════════════════════════════════════════════════════
//  ESTADO
// ═════════════════════════════════════════════════════════════════════════════
static volatile unsigned long totalPacketsSec = 0;

static unsigned long lastUpdate  = 0;
static unsigned long frameCount  = 0;

// Histórico circular
static int  history[HISTORY_SIZE];
static int  historyIdx   = 0;
static bool historyFull  = false;

// Estadísticas
static unsigned long totalEver   = 0;
static unsigned long sampleCount = 0;
static unsigned long sumPps      = 0;
static int peakPps               = 0;
static int currentPps            = 0;
static int smoothedPps           = 0;
static int lastDrawnPps          = -1;

static int monitorChannel = 1;

// Niveles (UMBRALES RECALIBRADOS)
enum ActivityLevel { LVL_QUIET, LVL_LOW, LVL_ACTIVE, LVL_BUSY, LVL_HEAVY, LVL_FLOODED };
static ActivityLevel currentLevel = LVL_QUIET;
static ActivityLevel lastLevel    = LVL_QUIET;

// Meter anim
static int meterHeight     = 0;
static int peakMeterHeight = 0;

// ═════════════════════════════════════════════════════════════════════════════
//  SNIFFER CALLBACK
// ═════════════════════════════════════════════════════════════════════════════
static void sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
    totalPacketsSec++;
}

// ═════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═════════════════════════════════════════════════════════════════════════════

// 🔧 UMBRALES RECALIBRADOS para entornos reales
//    Max típico en entorno normal: ~100-200 pps
//    Max con jammer activo: 250-500+ pps
static ActivityLevel classify(int pps) {
    if (pps < 5)    return LVL_QUIET;
    if (pps < 25)   return LVL_LOW;
    if (pps < 80)   return LVL_ACTIVE;
    if (pps < 150)  return LVL_BUSY;
    if (pps < 250)  return LVL_HEAVY;
    return LVL_FLOODED;     // 250+ = probable jamming/flood
}

static const char* levelLabel(ActivityLevel l) {
    switch (l) {
        case LVL_QUIET:   return "QUIET";
        case LVL_LOW:     return "LOW";
        case LVL_ACTIVE:  return "ACTIVE";
        case LVL_BUSY:    return "BUSY";
        case LVL_HEAVY:   return "HEAVY";
        case LVL_FLOODED: return "FLOODED";
    }
    return "";
}

static uint16_t levelColor(ActivityLevel l) {
    switch (l) {
        case LVL_QUIET:   return TFT_CYAN;
        case LVL_LOW:     return TFT_GREEN;
        case LVL_ACTIVE:  return TFT_GREEN;
        case LVL_BUSY:    return TFT_YELLOW;
        case LVL_HEAVY:   return TFT_ORANGE;
        case LVL_FLOODED: return TFT_RED;
    }
    return TFT_WHITE;
}

// Escala pps → altura. Tope ajustado a PPS_MAX_SCALE.
static int scaleToHeight(int pps, int maxH) {
    if (pps <= 0) return 0;
    if (pps > PPS_MAX_SCALE) pps = PPS_MAX_SCALE;
    float ratio = sqrt((float)pps / (float)PPS_MAX_SCALE);
    int h = (int)(ratio * maxH);
    if (h < 1 && pps > 0) h = 1;
    return h;
}

static int channelFreq(int ch) { return 2407 + ch * 5; }

// ═════════════════════════════════════════════════════════════════════════════
//  SONIDOS CORTOS (eventos)
// ═════════════════════════════════════════════════════════════════════════════
static void playStartupChirp() {
    beep(1200, 70); delay(30);
    beep(1800, 70); delay(30);
    beep(2400, 100);
}

static void playExitChirp() {
    beep(2400, 70); delay(30);
    beep(1800, 70); delay(30);
    beep(1200, 100);
}

static void playChannelBlip() {
    beep(2000, 25);
}

// ═════════════════════════════════════════════════════════════════════════════
//  SONIDO AMBIENTE (llamado continuamente en el loop)
//  🎵 Nueva lógica: patrones distintos por nivel, frecuencias en el sweet spot
//     del piezo para máxima intensidad percibida.
// ═════════════════════════════════════════════════════════════════════════════
static void updateAmbientSound(int pps) {
#if BUZZER_PIN < 0
    (void)pps;
    return;
#else
    static uint32_t phase = 0;
    phase++;

    if (!soundEnabled || pps < 5) {
        ledcWriteTone(0, 0);
        return;
    }

    int duty = map(soundVolume, 1, 5, 50, 255);
    ledcWrite(0, duty);

    if (pps < 25) {
        // LOW: tono suave, casi ambient
        ledcWriteTone(0, 1300 + random(-40, 60));
    }
    else if (pps < 80) {
        // ACTIVE: tono medio escalando con actividad
        int freq = map(pps, 25, 80, 1100, 1500);
        ledcWriteTone(0, freq + random(-50, 70));
    }
    else if (pps < 150) {
        // BUSY: heartbeat - dos pulsos + silencio (como latido tech)
        // Ciclo: tono1(150ms) - tono2(150ms) - silencio(300ms)
        int slot = (phase / 15) % 4;
        if      (slot == 0) ledcWriteTone(0, 1600 + random(-40, 40));
        else if (slot == 1) ledcWriteTone(0, 1300 + random(-40, 40));
        else                ledcWriteTone(0, 0);
    }
    else if (pps < 250) {
        // HEAVY: wobble rápido 8Hz, tono fuerte alarmante
        bool hi = (phase / 6) % 2 == 0;
        int freq = hi ? 1800 : 1200;
        ledcWriteTone(0, freq + random(-50, 50));
    }
    else {
        // FLOODED: SIRENA tipo ambulancia (~12Hz)
        // Las dos frecuencias (900 y 2400 Hz) están en el sweet spot del piezo
        // para máxima sonoridad → se escucha INTENSO y urgente.
        bool hi = (phase / 4) % 2 == 0;
        int freq = hi ? 2400 : 900;
        ledcWriteTone(0, freq + random(-80, 80));
    }
#endif
}

// ═════════════════════════════════════════════════════════════════════════════
//  DIBUJO
// ═════════════════════════════════════════════════════════════════════════════
static void drawFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);

    drawStringBig(8, 5, "PACKET MONITOR", TFT_WHITE, 1);

    tft.drawFastHLine(2, 26, 316, UI_ACCENT);
    tft.drawFastHLine(2, 136, 316, UI_ACCENT);
    tft.drawFastHLine(2, 208, 316, UI_ACCENT);

    tft.drawRect(METER_X - 1, METER_Y - 1, METER_W + 2, METER_H + 2, UI_ACCENT);
    tft.drawRect(HISTORY_X - 1, HISTORY_Y - 1, HISTORY_W + 2, HISTORY_H + 2, UI_ACCENT);

    drawStringCustom(8, 139, "HISTORY 60s", UI_ACCENT, 1);
    drawStringCustom(8, 218, "UP/DN: CH   OK(HOLD): EXIT", UI_ACCENT, 1);
}

static void drawChannel() {
    tft.fillRect(185, 3, 130, 20, TFT_BLACK);
    String chStr = "CH " + String(monitorChannel);
    drawStringBig(188, 5, chStr, TFT_CYAN, 1);
    drawStringCustom(245, 11, String(channelFreq(monitorChannel)) + "MHz",
                     UI_ACCENT, 1);
}

static void drawBigPps(int pps) {
    tft.fillRect(8, 33, 265, 60, TFT_BLACK);
    String pStr = String(pps);
    uint16_t col = levelColor(currentLevel);
    drawStringBig(14, 36, pStr, col, 3);
    drawStringCustom(14, 80, "PACKETS / SEC", UI_ACCENT, 1);
}

static void drawStatus() {
    tft.fillRect(6, 100, 270, 28, TFT_BLACK);
    uint16_t col = levelColor(currentLevel);

    // Blink del círculo cuando FLOODED
    bool showDot = true;
    if (currentLevel == LVL_FLOODED && (frameCount / 10) % 2 == 0) {
        showDot = false;
    }
    if (showDot) {
        tft.fillCircle(18, 113, 6, col);
        tft.drawCircle(18, 113, 6, TFT_WHITE);
    }
    drawStringBig(32, 104, levelLabel(currentLevel), col, 1);
}

static void drawMeter(int pps) {
    int target = scaleToHeight(pps, METER_H - 4);

    if (target > meterHeight) meterHeight = target;
    else                      meterHeight -= max(2, meterHeight / 8);
    if (meterHeight < 0) meterHeight = 0;

    if (meterHeight > peakMeterHeight) peakMeterHeight = meterHeight;
    else if (peakMeterHeight > 0)      peakMeterHeight -= 1;

    int y0 = METER_Y + METER_H - 2;
    int emptyTop = (METER_H - 4) - meterHeight;

    if (emptyTop > 0) {
        tft.fillRect(METER_X + 1, METER_Y + 1, METER_W - 2, emptyTop, TFT_BLACK);
    }

    for (int h = 0; h < meterHeight; h++) {
        uint16_t c;
        float ratio = (float)h / (METER_H - 4);
        if      (ratio < 0.40) c = TFT_GREEN;
        else if (ratio < 0.75) c = TFT_YELLOW;
        else                   c = TFT_RED;
        tft.drawFastHLine(METER_X + 2, y0 - h, METER_W - 4, c);
    }

    if (peakMeterHeight > meterHeight + 2) {
        tft.drawFastHLine(METER_X + 1, y0 - peakMeterHeight,
                          METER_W - 2, TFT_WHITE);
    }
}

static void pushHistory(int pps) {
    history[historyIdx] = pps;
    historyIdx = (historyIdx + 1) % HISTORY_SIZE;
    if (historyIdx == 0) historyFull = true;
}

static void drawHistory() {
    tft.fillRect(HISTORY_X, HISTORY_Y, HISTORY_W, HISTORY_H, TFT_BLACK);
    int total = historyFull ? HISTORY_SIZE : historyIdx;
    if (total == 0) return;
    int startIdx = historyFull ? historyIdx : 0;

    for (int i = 0; i < total; i++) {
        int idx = (startIdx + i) % HISTORY_SIZE;
        int pps = history[idx];
        int h = scaleToHeight(pps, HISTORY_H - 2);
        int bx = HISTORY_X + i * BAR_WIDTH;
        int by = HISTORY_Y + HISTORY_H - h;
        ActivityLevel l = classify(pps);
        uint16_t col = levelColor(l);
        if (h > 0) tft.fillRect(bx, by, BAR_WIDTH - 1, h, col);
    }
}

static void drawStats() {
    tft.fillRect(5, 190, 310, 15, TFT_BLACK);
    String stats = "TOT:" + String(totalEver) +
                   "  PEAK:" + String(peakPps) +
                   "  AVG:" + String(smoothedPps);
    drawStringCustom(8, 194, stats, TFT_WHITE, 1);
}

// ═════════════════════════════════════════════════════════════════════════════
//  MAIN
// ═════════════════════════════════════════════════════════════════════════════
void runPacketMonitor() {

    // Reset
    totalPacketsSec = 0;
    lastUpdate      = 0;
    historyIdx      = 0;
    historyFull     = false;
    totalEver       = 0;
    sampleCount     = 0;
    sumPps          = 0;
    peakPps         = 0;
    currentPps      = 0;
    smoothedPps     = 0;
    lastDrawnPps    = -1;
    meterHeight     = 0;
    peakMeterHeight = 0;
    monitorChannel  = 1;
    frameCount      = 0;
    currentLevel    = LVL_QUIET;
    lastLevel       = LVL_QUIET;
    memset(history, 0, sizeof(history));

#if BUZZER_PIN >= 0
    ledcSetup(0, 2000, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWriteTone(0, 0);
#endif

    drawFrame();
    drawChannel();
    drawBigPps(0);
    drawStatus();
    drawMeter(0);
    drawHistory();
    drawStats();

    playStartupChirp();

    WiFi.mode(WIFI_MODE_NULL);
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);

    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
    esp_wifi_set_channel(monitorChannel, WIFI_SECOND_CHAN_NONE);

    bool exitMonitor = false;

    while (!exitMonitor) {
        frameCount++;

        // ─── Cada segundo: capturar y actualizar displays ─────────────
        if (millis() - lastUpdate > 1000) {
            currentPps = totalPacketsSec;
            totalPacketsSec = 0;
            lastUpdate = millis();

            totalEver += currentPps;
            if (currentPps > peakPps) peakPps = currentPps;
            sampleCount++;
            sumPps += currentPps;
            smoothedPps = (int)(sumPps / sampleCount);

            lastLevel = currentLevel;
            currentLevel = classify(currentPps);

            if (currentPps != lastDrawnPps) {
                drawBigPps(currentPps);
                lastDrawnPps = currentPps;
            }
            drawStatus();
            pushHistory(currentPps);
            drawHistory();
            drawStats();
        }

        // ─── Sonido ambiente (cada loop, crea patrones) ───────────────
        updateAmbientSound(currentPps);

        // ─── Animación del meter (~33 fps) ────────────────────────────
        if (frameCount % 3 == 0) drawMeter(currentPps);

        // ─── Blink del status en FLOODED ──────────────────────────────
        if (currentLevel == LVL_FLOODED && frameCount % 10 == 0) {
            drawStatus();
        }

        // ─── CONTROLES ────────────────────────────────────────────────
        if (navUpPressed()) {
            if (monitorChannel < 13) {
                monitorChannel++;
                esp_wifi_set_channel(monitorChannel, WIFI_SECOND_CHAN_NONE);
                drawChannel();
                playChannelBlip();
            }
            delay(180);
        }
        if (navDownPressed()) {
            if (monitorChannel > 1) {
                monitorChannel--;
                esp_wifi_set_channel(monitorChannel, WIFI_SECOND_CHAN_NONE);
                drawChannel();
                playChannelBlip();
            }
            delay(180);
        }
        if (navEnterPressed()) {
            delay(300);
            if (navEnterPressed()) exitMonitor = true;
        }

        delay(10);
    }

    ledcWriteTone(0, 0);
    esp_wifi_set_promiscuous(false);
    playExitChirp();
    ledcWriteTone(0, 0);
}
