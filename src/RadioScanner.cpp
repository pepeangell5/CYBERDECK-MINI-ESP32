#include "RadioScanner.h"
#include "Settings.h"
#include "Pins.h"
#include "Input.h"
#include "PeripheralTools.h"
#include "SoundUtils.h"

// ═════════════════════════════════════════════════════════════════════════════
//  CONFIGURACIÓN
// ═════════════════════════════════════════════════════════════════════════════
#define SCAN_LIMIT       80      // 80 canales NRF (2.400 - 2.480 GHz)
#define SAMPLES_PER_CH   30      // muestras por canal para promediar
#define MAX_SAMPLE       30      // tope teórico de testCarrier hits

// ── Layout general ────────────────────────────────────────────────────────
#define SPECTRUM_GAIN    2

#define HEADER_H         34
#define FOOTER_H         22

// ── Spectrum mode layout ──────────────────────────────────────────────────
#define SPEC_Y_TOP       34
#define SPEC_Y_BOTTOM    200
#define SPEC_H           (SPEC_Y_BOTTOM - SPEC_Y_TOP)
#define SPEC_X_LEFT      10
#define SPEC_X_RIGHT     310
#define SPEC_W           (SPEC_X_RIGHT - SPEC_X_LEFT)

// ── Waterfall layout ──────────────────────────────────────────────────────
#define WF_Y_TOP         34
#define WF_Y_BOTTOM      200
#define WF_H             (WF_Y_BOTTOM - WF_Y_TOP)
#define WF_X_LEFT        10
#define WF_X_RIGHT       310

// ── Channel analyzer layout ───────────────────────────────────────────────
#define CH_Y_TOP         40
#define CH_Y_BOTTOM      180
#define CH_H             (CH_Y_BOTTOM - CH_Y_TOP)
#define CH_X_LEFT        14
#define CH_BAR_W         20
#define CH_BAR_GAP       2

// ═════════════════════════════════════════════════════════════════════════════
//  TABLA CANAL WIFI → CANAL NRF (idéntica a la de RadioJammer.cpp)
//  Índice 0 es placeholder (los canales WiFi empiezan en 1).
//  Usar la misma tabla garantiza que jammer y analyzer estén sincronizados.
// ═════════════════════════════════════════════════════════════════════════════
static const uint8_t wifiToNrfMap[14] = {
    0,  12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72
};

// ═════════════════════════════════════════════════════════════════════════════
//  ESTADO
// ═════════════════════════════════════════════════════════════════════════════
enum ScanMode { MODE_SPECTRUM = 0, MODE_WATERFALL = 1, MODE_CHANNEL = 2 };
static ScanMode currentMode = MODE_SPECTRUM;

static RF24 radio1(NRF1_CE_PIN, NRF1_CSN_PIN);
static RF24 radio2(NRF2_CE_PIN, NRF2_CSN_PIN);
static RF24* scanRadios[] = { &radio1, &radio2 };
static bool scanRadioOk[] = { false, false };
static const uint8_t scanRadioCount = sizeof(scanRadios) / sizeof(scanRadios[0]);

// Buffers
static int  samples[SCAN_LIMIT];        // última lectura cruda por canal NRF
static int  smoothSamples[SCAN_LIMIT];  // lectura filtrada para UI/analizador
static int  peaks[SCAN_LIMIT];          // peak hold por canal
static int  prevHeights[SCAN_LIMIT];    // altura animada (para spectrum)
static int  noiseFloor[SCAN_LIMIT];     // piso adaptativo por canal

// Stats globales
static int  peakChannel = 0;
static int  peakValue   = 0;
static int  globalNoise = 0;            // 0-100%

// Waterfall
#define WF_ROWS          (WF_H)
#define WF_COLS          (SCAN_LIMIT)
static uint8_t wfBuffer[WF_ROWS][WF_COLS];   // 0-255 intensidad por pixel
static int wfWriteRow = 0;
static bool wfFilled = false;

// Channel analyzer (13 canales WiFi)
static int wifiChanLevels[13];
static int wifiChanScores[13];
static int bestWifiChannel = 1;
static int lastRecommendedCh = -1;

// UI
static unsigned long frameCount = 0;
static bool spectrumNeedsClear = true;
static bool channelNeedsClear = true;

// RF baseline defense mode
static int rfBaseline[SCAN_LIMIT];
static int rfDelta[SCAN_LIMIT];
static int rfDeltaSmooth[SCAN_LIMIT];
static int rfWifiDelta[13];
static int rfDeltaPeak = 0;
static int rfDeltaPeakCh = 0;
static int rfChangedChannels = 0;
static int rfDriftScore = 0;

// ═════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═════════════════════════════════════════════════════════════════════════════

// NRF channel → frecuencia MHz
static inline int bandX(int left, int width, int index, int count) {
    return left + (index * width) / count;
}

static inline int bandW(int left, int width, int index, int count) {
    int x0 = bandX(left, width, index, count);
    int x1 = bandX(left, width, index + 1, count);
    int w = x1 - x0;
    return (w > 0) ? w : 1;
}

static inline int visualSample(int s) {
    int v = s * SPECTRUM_GAIN;
    if (v > MAX_SAMPLE) v = MAX_SAMPLE;
    if (v < 0) v = 0;
    return v;
}

static void resetAnalyzerState(bool clearWaterfall) {
    memset(samples, 0, sizeof(samples));
    memset(smoothSamples, 0, sizeof(smoothSamples));
    memset(peaks, 0, sizeof(peaks));
    memset(prevHeights, 0, sizeof(prevHeights));
    memset(noiseFloor, 0, sizeof(noiseFloor));
    memset(wifiChanLevels, 0, sizeof(wifiChanLevels));
    memset(wifiChanScores, 0, sizeof(wifiChanScores));

    if (clearWaterfall) {
        memset(wfBuffer, 0, sizeof(wfBuffer));
        wfWriteRow = 0;
        wfFilled = false;
    }

    globalNoise = 0;
    peakValue = 0;
    peakChannel = 0;
    lastRecommendedCh = -1;
    spectrumNeedsClear = true;
    channelNeedsClear = true;
}

static inline int nrfFreq(int ch) { return 2400 + ch; }

// NRF channel → canal WiFi (1-13) o 0 si fuera de rango.
// Usa wifiToNrfMap[] para consistencia con el jammer.
static int nrfToWifiCh(int nrfCh) {
    int bestCh = 0;
    int bestDist = 999;
    for (int w = 1; w <= 13; w++) {
        int wNrf = wifiToNrfMap[w];
        int d = abs(nrfCh - wNrf);
        if (d <= 3 && d < bestDist) {   // ventana ±3 para absorber desfases
            bestCh = w;
            bestDist = d;
        }
    }
    return bestCh;
}

// Sample (0-30) → color gradient
static uint16_t sampleToColor(int s) {
    if (s <= 0)  return TFT_BLACK;
    if (s < 5)   return 0x001F;     // azul
    if (s < 10)  return TFT_CYAN;
    if (s < 16)  return TFT_GREEN;
    if (s < 22)  return TFT_YELLOW;
    if (s < 27)  return TFT_ORANGE;
    return TFT_RED;
}

// Color según altura relativa dentro de una barra (para gradient vertical)
static uint16_t heightGradient(float ratio) {
    if (ratio < 0.33) return TFT_GREEN;
    if (ratio < 0.66) return TFT_YELLOW;
    if (ratio < 0.85) return TFT_ORANGE;
    return TFT_RED;
}

// Texto descriptor del estado de un canal WiFi según su nivel
static const char* wifiStatus(int level) {
    if (level < 4)   return "CLEAN";
    if (level < 10)  return "OK";
    if (level < 18)  return "BUSY";
    return "CROWDED";
}
static uint16_t wifiStatusColor(int level) {
    if (level < 4)   return TFT_GREEN;
    if (level < 10)  return TFT_CYAN;
    if (level < 18)  return TFT_YELLOW;
    return TFT_RED;
}

// ═════════════════════════════════════════════════════════════════════════════
//  SNIFFING DEL ESPECTRO (una pasada completa)
// ═════════════════════════════════════════════════════════════════════════════
static bool doFullSweep() {
    long totalHits = 0;
    peakValue   = 0;
    peakChannel = 0;

    memset(samples, 0, sizeof(samples));

    uint8_t activeRadios[2];
    uint8_t activeCount = 0;
    for (uint8_t r = 0; r < scanRadioCount; r++) {
        if (scanRadioOk[r]) activeRadios[activeCount++] = r;
    }
    if (activeCount == 0) return false;

    for (int base = 0; base < SCAN_LIMIT; base += activeCount) {
        if (isBackPressed()) return false;

        for (uint8_t slot = 0; slot < activeCount; slot++) {
            uint8_t r = activeRadios[slot];
            int ch = base + slot;
            if (ch >= SCAN_LIMIT || !scanRadioOk[r]) continue;
            scanRadios[r]->setChannel(ch);
            scanRadios[r]->startListening();
        }
        delayMicroseconds(130);

        int channelSamples[2] = { 0 };
        for (int k = 0; k < SAMPLES_PER_CH; k++) {
            if (isBackPressed()) return false;

            for (uint8_t slot = 0; slot < activeCount; slot++) {
                uint8_t r = activeRadios[slot];
                int ch = base + slot;
                if (ch >= SCAN_LIMIT || !scanRadioOk[r]) continue;
                if (scanRadios[r]->testCarrier()) channelSamples[slot]++;
            }
            delayMicroseconds(20);
        }

        for (uint8_t slot = 0; slot < activeCount; slot++) {
            uint8_t r = activeRadios[slot];
            int i = base + slot;
            if (i >= SCAN_LIMIT || !scanRadioOk[r]) continue;

            scanRadios[r]->stopListening();

            int raw = channelSamples[slot];
            samples[i] = raw;

            int floor = noiseFloor[i];
            if (raw < floor || frameCount < 10) {
                floor = (floor * 5 + raw) / 6;
            } else {
                floor = (floor * 31 + raw) / 32;
            }
            noiseFloor[i] = floor;

            int cleaned = raw - (floor / 2);
            if (cleaned < 0) cleaned = 0;
            int boosted = visualSample(cleaned);

            if (frameCount < 2) smoothSamples[i] = boosted;
            else smoothSamples[i] = (smoothSamples[i] * 3 + boosted * 2) / 5;

            int v = smoothSamples[i];

            // Peak hold con decay lento
            if (v > peaks[i]) peaks[i] = v;
            else if (peaks[i] > 0 && (frameCount % 4 == 0)) peaks[i]--;

            totalHits += v;

            if (v > peakValue) { peakValue = v; peakChannel = i; }
        }
    }

    // Ruido global como porcentaje (max teórico = SCAN_LIMIT * MAX_SAMPLE)
    long maxPossible = (long)SCAN_LIMIT * MAX_SAMPLE;
    globalNoise = (int)((totalHits * 100L) / maxPossible);
    if (globalNoise > 100) globalNoise = 100;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  CÁLCULO DE NIVELES POR CANAL WIFI
//  - Ventana ampliada a ±3 canales NRF con pesos (centro pesa más)
//  - Toma max(promedio_ponderado, pico) para que ataques puntuales no se diluyan
//  - Usa wifiToNrfMap[] para sincronía exacta con el jammer
// ═════════════════════════════════════════════════════════════════════════════
static void computeWifiChannels() {
    long best = 0x7FFFFFFF;
    int candidateBest = bestWifiChannel;

    for (int w = 1; w <= 13; w++) {
        int centerNrf = wifiToNrfMap[w];

        long weighted    = 0;
        long totalWeight = 0;
        int  maxInWindow = 0;

        for (int d = -5; d <= 5; d++) {
            int nrfCh = centerNrf + d;
            if (nrfCh < 0 || nrfCh >= SCAN_LIMIT) continue;

            int sample = smoothSamples[nrfCh];

            // Pesos: centro=4, ±1=3, ±2=2, ±3=1
            int weight = 6 - abs(d);
            if (weight < 1) weight = 1;

            weighted += (long)sample * weight;
            totalWeight += weight;

            if (sample > maxInWindow) maxInWindow = sample;
        }

        int avg   = totalWeight ? (int)(weighted / totalWeight) : 0;
        int level = (maxInWindow > avg) ? maxInWindow : avg;
        wifiChanLevels[w - 1] = (wifiChanLevels[w - 1] * 2 + level) / 3;
        long score = weighted + ((long)maxInWindow * 12);
        wifiChanScores[w - 1] = totalWeight ? (int)(score / totalWeight) : 0;

        // El "mejor canal" es el de menor actividad ponderada total
        if (score < best) {
            best = score;
            candidateBest = w;
        }
    }

    int currentScore = wifiChanScores[bestWifiChannel - 1];
    int nextScore = wifiChanScores[candidateBest - 1];
    if (candidateBest != bestWifiChannel) {
        if (frameCount < 4 || nextScore + 2 < currentScore || (frameCount % 16 == 0)) {
            bestWifiChannel = candidateBest;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  SONIDO
// ═════════════════════════════════════════════════════════════════════════════
static unsigned long lastGeigerTick = 0;

static void playModeSwitch() {
    beep(1800, 40); delay(15);
    beep(2400, 60);
}

static void playStartup() {
    beep(1200, 50); delay(20);
    beep(1800, 50); delay(20);
    beep(2400, 80);
}

static void playExit() {
    beep(2400, 50); delay(20);
    beep(1800, 50); delay(20);
    beep(1200, 80);
}

// Geiger ambient (solo en SPECTRUM). Se llama desde el loop principal.
static void geigerAmbient(int intensity) {
#if BUZZER_PIN < 0
    (void)intensity;
    return;
#else
    if (!soundEnabled || intensity < 2) {
        ledcWriteTone(0, 0);
        return;
    }
    int duty = map(soundVolume, 1, 5, 50, 255);
    ledcWrite(0, duty);

    // Frecuencia base en el sweet spot del piezo
    int freq;
    if      (intensity < 6)  freq = 1300;
    else if (intensity < 12) freq = 1600;
    else if (intensity < 20) freq = 1900;
    else                     freq = 2200;
    freq += random(-80, 120);

    // Clicks a bajo nivel, continuo a alto nivel
    int interval = map(intensity, 2, MAX_SAMPLE, 180, 8);
    if (interval > 40) {
        if (millis() - lastGeigerTick > (unsigned long)interval) {
            ledcWriteTone(0, freq);
            delay(3);
            ledcWriteTone(0, 0);
            lastGeigerTick = millis();
        }
    } else {
        ledcWriteTone(0, freq);
    }
#endif
}

static void configureScannerRadio(RF24& radio) {
    radio.powerUp();
    radio.setAutoAck(false);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_1MBPS);
    radio.startListening();
    radio.stopListening();
}

static void prepareScannerDisplay() {
    pinMode(TFT_CS_PIN, OUTPUT);
    pinMode(NRF1_CSN_PIN, OUTPUT);
    pinMode(NRF2_CSN_PIN, OUTPUT);
    pinMode(NRF1_CE_PIN, OUTPUT);
    pinMode(NRF2_CE_PIN, OUTPUT);
    digitalWrite(NRF1_CE_PIN, LOW);
    digitalWrite(NRF2_CE_PIN, LOW);
    digitalWrite(NRF1_CSN_PIN, HIGH);
    digitalWrite(NRF2_CSN_PIN, HIGH);
    digitalWrite(TFT_CS_PIN, HIGH);
    delayMicroseconds(80);
}

static void hardClearScannerDisplay() {
    prepareScannerDisplay();
    tft.fillScreen(TFT_BLACK);
    delay(12);
    tft.fillRect(0, 0, 320, 240, TFT_BLACK);
}

static void cleanupScanner(bool clearScreen) {
    ledcWriteTone(0, 0);

    for (uint8_t r = 0; r < scanRadioCount; r++) {
        if (scanRadioOk[r]) {
            scanRadios[r]->stopListening();
            scanRadios[r]->powerDown();
        }
    }

    digitalWrite(NRF1_CE_PIN, LOW);
    digitalWrite(NRF2_CE_PIN, LOW);
    digitalWrite(NRF1_CSN_PIN, HIGH);
    digitalWrite(NRF2_CSN_PIN, HIGH);
    digitalWrite(TFT_CS_PIN, HIGH);

    if (clearScreen) {
        hardClearScannerDisplay();
        delay(25);
    }
}

static bool initScannerRadios() {
#if BUZZER_PIN >= 0
    ledcSetup(0, 2000, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWriteTone(0, 0);
#endif

    pinMode(TFT_CS_PIN, OUTPUT);
    digitalWrite(TFT_CS_PIN, HIGH);
    pinMode(NRF1_CSN_PIN, OUTPUT);
    digitalWrite(NRF1_CSN_PIN, HIGH);
    pinMode(NRF2_CSN_PIN, OUTPUT);
    digitalWrite(NRF2_CSN_PIN, HIGH);
    pinMode(NRF1_CE_PIN, OUTPUT);
    digitalWrite(NRF1_CE_PIN, LOW);
    pinMode(NRF2_CE_PIN, OUTPUT);
    digitalWrite(NRF2_CE_PIN, LOW);
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

    scanRadioOk[0] = radio1.begin();
    if (scanRadioOk[0]) configureScannerRadio(radio1);

    scanRadioOk[1] = radio2.begin();
    if (scanRadioOk[1]) configureScannerRadio(radio2);

    Serial.printf("[RadioScanner] NRF1 CE:%d CSN:%d -> %s\n",
                  NRF1_CE_PIN, NRF1_CSN_PIN, scanRadioOk[0] ? "OK" : "FAIL");
    Serial.printf("[RadioScanner] NRF2 CE:%d CSN:%d -> %s\n",
                  NRF2_CE_PIN, NRF2_CSN_PIN, scanRadioOk[1] ? "OK" : "FAIL");

    return scanRadioOk[0] || scanRadioOk[1];
}

static void drawScannerErrorScreen() {
    hardClearScannerDisplay();
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(45, 90, "NRF24 ERROR", TFT_RED, 2);
    drawStringCustom(30, 130, "NRF1 CE:" + String(NRF1_CE_PIN) + " CSN:" + String(NRF1_CSN_PIN), UI_ACCENT, 1);
    drawStringCustom(30, 145, "NRF2 CE:" + String(NRF2_CE_PIN) + " CSN:" + String(NRF2_CSN_PIN), UI_ACCENT, 1);
    drawStringCustom(30, 160, "SPI " + String(SCK_PIN) + "/" + String(MISO_PIN) + "/" + String(MOSI_PIN), UI_ACCENT, 1);
    drawStringCustom(30, 190, "OK/BACK: RETURN", UI_ACCENT, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput();
}

// ═════════════════════════════════════════════════════════════════════════════
//  HEADER COMÚN (a todos los modos)
// ═════════════════════════════════════════════════════════════════════════════
static void drawHeader(const char* title, int modeNum) {
    tft.fillRect(0, 0, 320, HEADER_H, TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);

    // Title con fuente BIG
    drawStringBig(6, 4, title, TFT_WHITE, 1);

    // Mode indicator
    String modeTag = "MODE " + String(modeNum) + "/3";
    drawStringRight(306, 6, modeTag, UI_ACCENT, 1);

    // Meter de ruido global (4 bloques) a la derecha
    uint16_t col = (globalNoise < 30) ? TFT_GREEN :
                   (globalNoise < 60) ? TFT_YELLOW :
                   (globalNoise < 85) ? TFT_ORANGE : TFT_RED;

    drawStringCustom(8, 23, "NRF 2.4GHz", UI_ACCENT, 1);
    drawStringRight(306, 23, "NOISE " + String(globalNoise) + "%", col, 1);

    // Línea divisoria
    tft.drawFastHLine(0, HEADER_H - 1, 320, UI_ACCENT);
}

static void drawFooter(const char* leftInfo) {
    tft.fillRect(1, 240 - FOOTER_H, 318, FOOTER_H - 1, TFT_BLACK);
    tft.drawFastHLine(0, 240 - FOOTER_H - 1, 320, UI_ACCENT);

    drawStringCustom(6, 240 - FOOTER_H + 4, leftInfo, UI_ACCENT, 1);
    drawStringCustom(6, 240 - FOOTER_H + 13,
        "UP/DN/ENC:MODE   BACK/OK(H):EXIT", UI_ACCENT, 1);
}

// ═════════════════════════════════════════════════════════════════════════════
//  MODO 1: SPECTRUM
// ═════════════════════════════════════════════════════════════════════════════
static void drawSpectrumGrid() {
    tft.fillRect(SPEC_X_LEFT, SPEC_Y_TOP, SPEC_W, SPEC_H, TFT_BLACK);

    // Grid horizontal sutil
    for (int i = 1; i < 4; i++) {
        int y = SPEC_Y_TOP + (SPEC_H * i) / 4;
        for (int x = SPEC_X_LEFT; x < SPEC_X_RIGHT; x += 6) {
            tft.drawPixel(x, y, UI_ACCENT);
        }
    }

    // Labels de canales WiFi (1, 6, 11 destacados)
    int labelY = SPEC_Y_BOTTOM + 2;
    tft.drawFastHLine(SPEC_X_LEFT, SPEC_Y_BOTTOM, SPEC_W, TFT_WHITE);

    int wifiChansToShow[] = {1, 6, 11, 13};
    for (int w : wifiChansToShow) {
        int nrfCh = wifiToNrfMap[w];
        int x = SPEC_X_LEFT + (nrfCh * SPEC_W) / SCAN_LIMIT;
        tft.drawFastVLine(x, SPEC_Y_BOTTOM, 3, TFT_WHITE);
        String lbl = String(w);
        int lblW = getTextWidth(lbl, 1);
        drawStringCustom(x - lblW/2, labelY + 2, lbl, TFT_WHITE, 1);
    }

    drawStringCustom(SPEC_X_LEFT, labelY + 10, "2.400", UI_ACCENT, 1);
    drawStringCustom(SPEC_X_RIGHT - 30, labelY + 10, "2.480GHz", UI_ACCENT, 1);
}

static void drawSpectrumFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawSpectrumGrid();
    spectrumNeedsClear = false;
}

static void drawSpectrumBars() {
    drawSpectrumGrid();
    spectrumNeedsClear = false;

    for (int i = 0; i < SCAN_LIMIT; i++) {
        int s = smoothSamples[i];
        int target = map(s, 0, MAX_SAMPLE, 0, SPEC_H);
        if (target > SPEC_H) target = SPEC_H;

        // Smoothing: subida instantánea, bajada suave
        int ph = prevHeights[i];
        if (target > ph) ph = target;
        else             ph -= (ph / 6) + 1;
        if (ph < 0) ph = 0;
        prevHeights[i] = ph;

        int x = bandX(SPEC_X_LEFT, SPEC_W, i, SCAN_LIMIT);
        int barW = bandW(SPEC_X_LEFT, SPEC_W, i, SCAN_LIMIT);

        // Borrar área completa encima de la barra
        tft.fillRect(x, SPEC_Y_TOP, barW, SPEC_H - ph, TFT_BLACK);

        // Redibujar grid horizontal en la zona negra
        for (int g = 1; g < 4; g++) {
            int gy = SPEC_Y_TOP + (SPEC_H * g) / 4;
            if (gy < SPEC_Y_TOP + (SPEC_H - ph)) {
                for (int gx = x; gx < x + barW && gx < SPEC_X_RIGHT; gx += 6) {
                    tft.drawPixel(gx, gy, UI_ACCENT);
                }
            }
        }

        // Dibujar barra con gradient vertical
        if (ph > 0) {
            for (int py = 0; py < ph; py++) {
                float ratio = (float)py / SPEC_H;
                uint16_t col = heightGradient(ratio);
                tft.drawFastHLine(x, SPEC_Y_BOTTOM - py, barW, col);
            }
        }

        // Peak hold (línea blanca)
        int peakH = map(peaks[i], 0, MAX_SAMPLE, 0, SPEC_H);
        if (peakH > SPEC_H) peakH = SPEC_H;
        if (peakH > ph + 2) {
            tft.drawFastHLine(x, SPEC_Y_BOTTOM - peakH, barW, TFT_WHITE);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  MODO 2: WATERFALL
// ═════════════════════════════════════════════════════════════════════════════
static void drawWaterfallFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);

    int labelY = WF_Y_BOTTOM + 2;
    tft.drawFastHLine(WF_X_LEFT, WF_Y_BOTTOM, WF_X_RIGHT - WF_X_LEFT, TFT_WHITE);

    int wifiChansToShow[] = {1, 6, 11, 13};
    for (int w : wifiChansToShow) {
        int nrfCh = wifiToNrfMap[w];
        int x = WF_X_LEFT + (nrfCh * (WF_X_RIGHT - WF_X_LEFT)) / SCAN_LIMIT;
        tft.drawFastVLine(x, WF_Y_BOTTOM, 3, TFT_WHITE);
        String lbl = String(w);
        int lblW = getTextWidth(lbl, 1);
        drawStringCustom(x - lblW/2, labelY + 2, lbl, TFT_WHITE, 1);
    }
    drawStringCustom(WF_X_LEFT, labelY + 10, "2.400", UI_ACCENT, 1);
    drawStringCustom(WF_X_RIGHT - 30, labelY + 10, "2.480GHz", UI_ACCENT, 1);

    memset(wfBuffer, 0, sizeof(wfBuffer));
    wfWriteRow = 0;
    wfFilled = false;
}

// Push fila nueva al waterfall (ring buffer)
static void waterfallPush() {
    for (int i = 0; i < WF_COLS; i++) {
        int s = smoothSamples[i];
        int v = (s * 255) / MAX_SAMPLE;
        if (v > 255) v = 255;
        wfBuffer[wfWriteRow][i] = (uint8_t)v;
    }
    wfWriteRow = (wfWriteRow + 1) % WF_ROWS;
    if (wfWriteRow == 0) wfFilled = true;
}

// Renderizar waterfall completo (más nueva arriba, más vieja abajo)
static void waterfallRender() {
    int wfPxWidth = WF_X_RIGHT - WF_X_LEFT;
    int rowsToRender = wfFilled ? WF_ROWS : wfWriteRow;
    if (rowsToRender < WF_ROWS) {
        tft.fillRect(WF_X_LEFT, WF_Y_TOP + rowsToRender,
                     wfPxWidth, WF_ROWS - rowsToRender, TFT_BLACK);
    }

    for (int visRow = 0; visRow < rowsToRender; visRow++) {
        int bufRow = (wfWriteRow - 1 - visRow + WF_ROWS) % WF_ROWS;
        int screenY = WF_Y_TOP + visRow;

        for (int c = 0; c < SCAN_LIMIT; c++) {
            uint8_t v = wfBuffer[bufRow][c];
            int s = (v * MAX_SAMPLE) / 255;
            uint16_t col = sampleToColor(s);
            int x = bandX(WF_X_LEFT, wfPxWidth, c, SCAN_LIMIT);
            int barW = bandW(WF_X_LEFT, wfPxWidth, c, SCAN_LIMIT);
            if (barW == 1) {
                tft.drawPixel(x, screenY, col);
            } else {
                tft.drawFastHLine(x, screenY, barW, col);
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  MODO 3: CHANNEL ANALYZER
// ═════════════════════════════════════════════════════════════════════════════
static void drawChannelFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);

    // Grid de fondo
    for (int i = 1; i < 4; i++) {
        int y = CH_Y_TOP + (CH_H * i) / 4;
        for (int x = CH_X_LEFT; x < CH_X_LEFT + 13*(CH_BAR_W + CH_BAR_GAP); x += 6) {
            tft.drawPixel(x, y, UI_ACCENT);
        }
    }

    tft.drawFastHLine(CH_X_LEFT - 4, CH_Y_BOTTOM, 13*(CH_BAR_W+CH_BAR_GAP)+4, TFT_WHITE);

    // Labels de canales (1-13)
    for (int w = 1; w <= 13; w++) {
        int cx = CH_X_LEFT + (w-1)*(CH_BAR_W + CH_BAR_GAP);
        String lbl = String(w);
        int lblW = getTextWidth(lbl, 1);
        drawStringCustom(cx + (CH_BAR_W - lblW)/2, CH_Y_BOTTOM + 3, lbl, TFT_WHITE, 1);
    }
}

static void drawChannelBars() {
    if (channelNeedsClear) {
        tft.fillRect(CH_X_LEFT - 4, CH_Y_TOP - 2,
                     13 * (CH_BAR_W + CH_BAR_GAP) + 8, CH_H + 4, TFT_BLACK);
        channelNeedsClear = false;
    }

    for (int w = 1; w <= 13; w++) {
        int level = wifiChanLevels[w - 1];
        int h = map(level, 0, MAX_SAMPLE, 0, CH_H);
        if (h > CH_H) h = CH_H;

        int cx = CH_X_LEFT + (w-1)*(CH_BAR_W + CH_BAR_GAP);

        tft.fillRect(cx - 2, CH_Y_TOP - 2, CH_BAR_W + 4, CH_H + 4, TFT_BLACK);

        // Redibujar grid en la zona no cubierta
        for (int g = 1; g < 4; g++) {
            int gy = CH_Y_TOP + (CH_H * g) / 4;
            if (gy < CH_Y_BOTTOM - h) {
                for (int gx = cx; gx < cx + CH_BAR_W; gx += 6) {
                    tft.drawPixel(gx, gy, UI_ACCENT);
                }
            }
        }

        // Dibujar barra con gradient
        if (h > 0) {
            for (int py = 0; py < h; py++) {
                float ratio = (float)py / CH_H;
                uint16_t col = heightGradient(ratio);
                tft.drawFastHLine(cx, CH_Y_BOTTOM - py, CH_BAR_W, col);
            }
        }

        // Marcar el mejor canal con borde cyan
        if (w == bestWifiChannel) {
            tft.drawRect(cx - 1, CH_Y_TOP - 1, CH_BAR_W + 2, CH_H + 2, TFT_CYAN);
        }
    }

    // Recomendación debajo
    tft.drawFastHLine(CH_X_LEFT - 4, CH_Y_BOTTOM, 13*(CH_BAR_W+CH_BAR_GAP)+4, TFT_WHITE);
    tft.fillRect(5, 200, 310, 14, TFT_BLACK);
    int bestLevel = wifiChanLevels[bestWifiChannel - 1];
    String rec = "BEST: CH " + String(bestWifiChannel) +
                 " (" + String(wifiStatus(bestLevel)) + ")";
    drawStringCustom(8, 203, rec, wifiStatusColor(bestLevel), 2);
}

// ═════════════════════════════════════════════════════════════════════════════
//  DISPATCHERS POR MODO
// ═════════════════════════════════════════════════════════════════════════════
static void drawCurrentFrame() {
    switch (currentMode) {
        case MODE_SPECTRUM:   drawSpectrumFrame();  break;
        case MODE_WATERFALL:  drawWaterfallFrame(); break;
        case MODE_CHANNEL:    drawChannelFrame();   break;
    }
}

static void drawCurrentHeader() {
    switch (currentMode) {
        case MODE_SPECTRUM:
            drawHeader("SPECTRUM", 1);
            break;
        case MODE_WATERFALL:
            drawHeader("WATERFALL", 2);
            break;
        case MODE_CHANNEL:
            drawHeader("WIFI CHANS", 3);
            break;
    }
}

static void drawCurrentFooter() {
    String peakInfo;
    if (currentMode == MODE_CHANNEL) {
        peakInfo = "Active WiFi: CH1-13 only";
    } else {
        int peakWifi = nrfToWifiCh(peakChannel);
        int pct = (peakValue * 100) / MAX_SAMPLE;
        peakInfo = "PEAK: NRF" + String(peakChannel);
        if (peakWifi > 0) peakInfo = peakInfo + " (WiFi " + String(peakWifi) + ")";
        peakInfo = peakInfo + "  " + String(pct) + "%";
    }
    drawFooter(peakInfo.c_str());
}

static void drawCurrentData() {
    switch (currentMode) {
        case MODE_SPECTRUM:   drawSpectrumBars();   break;
        case MODE_WATERFALL:
            waterfallPush();
            waterfallRender();
            break;
        case MODE_CHANNEL:    drawChannelBars();    break;
    }
}

// Cambio de modo
static void switchMode(ScanMode newMode) {
    currentMode = newMode;
    memset(prevHeights, 0, sizeof(prevHeights));
    memset(peaks, 0, sizeof(peaks));
    memset(wfBuffer, 0, sizeof(wfBuffer));
    wfWriteRow = 0;
    wfFilled = false;
    lastRecommendedCh = -1;
    spectrumNeedsClear = true;
    channelNeedsClear = true;

    drawCurrentFrame();
    playModeSwitch();
}

// ═════════════════════════════════════════════════════════════════════════════
//  MAIN
// ═════════════════════════════════════════════════════════════════════════════
static void drawScannerInitScreen() {
    hardClearScannerDisplay();
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(20, 78, "RADIO INIT", TFT_WHITE, 2);
    drawStringCustom(36, 124, "NRF24 spectrum analyzer", UI_ACCENT, 1);
    drawStringCustom(58, 146, "Preparando radios...", UI_ACCENT, 1);
}

static const char* rfRiskLabel() {
    if (rfDriftScore < 20) return "LOW";
    if (rfDriftScore < 45) return "WATCH";
    if (rfDriftScore < 70) return "RISK";
    return "ALERT";
}

static uint16_t rfRiskColor() {
    if (rfDriftScore < 20) return TFT_GREEN;
    if (rfDriftScore < 45) return TFT_CYAN;
    if (rfDriftScore < 70) return TFT_YELLOW;
    return TFT_RED;
}

static void drawRfFrame(const char* title) {
    hardClearScannerDisplay();
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(8, 8, title, TFT_WHITE, 1);
    drawStringRight(308, 12, "DEFENSE", UI_ACCENT, 1);
    tft.drawFastHLine(0, 34, 320, TFT_WHITE);
    tft.drawFastHLine(0, 214, 320, TFT_WHITE);
}

static void drawRfProgress(int step, int total, const char* msg) {
    drawRfFrame("RF BASELINE");
    tft.fillRect(1, 35, 318, 178, TFT_BLACK);
    drawStringCustom(26, 70, msg, TFT_CYAN, 2);
    drawStringCustom(26, 98, "Keep radio environment normal", UI_ACCENT, 1);

    int barX = 24;
    int barY = 130;
    int barW = 272;
    int fillW = (step * (barW - 2)) / max(1, total);
    tft.drawRect(barX, barY, barW, 14, TFT_WHITE);
    if (fillW > 0) tft.fillRect(barX + 1, barY + 1, fillW, 12, TFT_CYAN);
    drawStringCustom(26, 158, String(step) + "/" + String(total), TFT_WHITE, 2);
    drawStringCustom(10, 222, "BACK: CANCEL", UI_ACCENT, 1);
}

static bool captureRfBaseline(int rounds) {
    hardClearScannerDisplay();
    memset(rfBaseline, 0, sizeof(rfBaseline));
    memset(rfDelta, 0, sizeof(rfDelta));
    memset(rfDeltaSmooth, 0, sizeof(rfDeltaSmooth));
    memset(rfWifiDelta, 0, sizeof(rfWifiDelta));
    rfDeltaPeak = 0;
    rfDeltaPeakCh = 0;
    rfChangedChannels = 0;
    rfDriftScore = 0;

    resetAnalyzerState(false);

    for (int r = 0; r < rounds; r++) {
        if (isBackPressed()) return false;
        drawRfProgress(r + 1, rounds, "Capturing baseline");
        frameCount++;
        if (!doFullSweep()) return false;
        for (int i = 0; i < SCAN_LIMIT; i++) {
            rfBaseline[i] += samples[i];
        }
        beep(1600 + r * 35, 12);
        delay(20);
    }

    for (int i = 0; i < SCAN_LIMIT; i++) {
        rfBaseline[i] = (rfBaseline[i] + rounds / 2) / rounds;
    }
    return true;
}

static void computeRfDelta() {
    memset(rfWifiDelta, 0, sizeof(rfWifiDelta));
    rfDeltaPeak = 0;
    rfDeltaPeakCh = 0;
    rfChangedChannels = 0;
    long totalDelta = 0;

    for (int i = 0; i < SCAN_LIMIT; i++) {
        int d = samples[i] - rfBaseline[i] - 2;
        if (d < 0) d = 0;
        if (d > MAX_SAMPLE) d = MAX_SAMPLE;
        rfDelta[i] = d;
        rfDeltaSmooth[i] = (rfDeltaSmooth[i] * 2 + d * 3) / 5;

        int v = rfDeltaSmooth[i];
        totalDelta += v;
        if (v >= 5) rfChangedChannels++;
        if (v > rfDeltaPeak) {
            rfDeltaPeak = v;
            rfDeltaPeakCh = i;
        }

        int wifiCh = nrfToWifiCh(i);
        if (wifiCh > 0) rfWifiDelta[wifiCh - 1] += v;
    }

    int density = (int)((totalDelta * 100L) / (SCAN_LIMIT * 10L));
    int spread = rfChangedChannels * 2;
    rfDriftScore = density + spread;
    if (rfDeltaPeak >= 14) rfDriftScore += 18;
    if (rfDeltaPeak >= 22) rfDriftScore += 18;
    if (rfDriftScore > 100) rfDriftScore = 100;
}

static int strongestWifiDeltaChannel() {
    int best = 1;
    int bestVal = rfWifiDelta[0];
    for (int i = 1; i < 13; i++) {
        if (rfWifiDelta[i] > bestVal) {
            bestVal = rfWifiDelta[i];
            best = i + 1;
        }
    }
    return best;
}

static void drawRfBaselineLive() {
    const int statusX = 8;
    const int statusY = 40;
    const int statusW = 88;
    const int statusH = 38;
    const int metricX = 104;
    const int plotX = 10;
    const int plotY = 112;
    const int plotW = 300;
    const int plotH = 72;
    const int axisY = plotY + plotH + 5;
    const int summaryY = 202;
    uint16_t riskCol = rfRiskColor();

    prepareScannerDisplay();
    tft.fillRect(1, 35, 318, 178, TFT_BLACK);
    tft.drawFastHLine(8, 102, 304, UI_ACCENT);

    tft.fillRect(statusX - 1, statusY - 1, statusW + 2, statusH + 2, TFT_BLACK);
    tft.drawRect(statusX, statusY, statusW, statusH, riskCol);
    drawStringFit(statusX + 8, statusY + 12, rfRiskLabel(), riskCol,
                  statusW - 16, 1, FONT_BIG);

    tft.fillRect(metricX - 2, statusY - 2, 210, 54, TFT_BLACK);
    drawStringFit(metricX, statusY + 0,
        "SCORE " + String(rfDriftScore) + "/100", riskCol, 204, 1);
    drawStringFit(metricX, statusY + 16,
        "PEAK NRF" + String(rfDeltaPeakCh) + " +" + String(rfDeltaPeak),
        TFT_WHITE, 204, 1);

    int peakWifi = nrfToWifiCh(rfDeltaPeakCh);
    String wifiLine = "WIFI ";
    wifiLine += (peakWifi > 0) ? ("CH" + String(peakWifi)) : String("--");
    wifiLine += "  SPREAD " + String(rfChangedChannels);
    drawStringFit(metricX, statusY + 32, wifiLine, UI_ACCENT, 204, 1);

    tft.fillRect(plotX, plotY, plotW, plotH, TFT_BLACK);

    for (int g = 1; g < 4; g++) {
        int y = plotY + (plotH * g) / 4;
        for (int x = plotX; x < plotX + plotW; x += 6) tft.drawPixel(x, y, UI_ACCENT);
    }
    tft.drawFastHLine(plotX, plotY + plotH, plotW, TFT_WHITE);

    for (int i = 0; i < SCAN_LIMIT; i++) {
        int x = bandX(plotX, plotW, i, SCAN_LIMIT);
        int w = bandW(plotX, plotW, i, SCAN_LIMIT);

        int baseH = map(rfBaseline[i], 0, MAX_SAMPLE, 0, plotH);
        if (baseH > plotH) baseH = plotH;
        if (baseH > 0) {
            uint16_t baseCol = 0x03EF;
            tft.drawFastHLine(x, plotY + plotH - baseH, w, baseCol);
        }

        int dH = map(rfDeltaSmooth[i], 0, MAX_SAMPLE, 0, plotH);
        if (dH > plotH) dH = plotH;
        if (dH > 0) {
            uint16_t col = sampleToColor(rfDeltaSmooth[i] + 4);
            tft.fillRect(x, plotY + plotH - dH, w, dH, col);
        }
    }

    int wifiChansToShow[] = {1, 6, 11, 13};
    for (int w : wifiChansToShow) {
        int nrfCh = wifiToNrfMap[w];
        int x = plotX + (nrfCh * plotW) / SCAN_LIMIT;
        tft.drawFastVLine(x, plotY + plotH, 3, TFT_WHITE);
        drawStringCustom(x - 3, axisY, String(w), TFT_WHITE, 1);
    }

    int strongWifi = strongestWifiDeltaChannel();
    tft.fillRect(1, summaryY - 2, 318, 14, TFT_BLACK);
    drawStringFit(10, summaryY, "STRONGEST WIFI CH " + String(strongWifi),
                  rfWifiDelta[strongWifi - 1] > 20 ? TFT_YELLOW : TFT_CYAN,
                  300, 1);
    tft.fillRect(1, 215, 318, 24, TFT_BLACK);
    drawStringCustom(10, 222, "UP:NEW BASE  OK:SAVE  BACK:EXIT", UI_ACCENT, 1);
}

static bool exportRfBaselineReport() {
    String out;
    out.reserve(2048);
    out += "CYBERDECK RF BASELINE REPORT\r\n";
    out += "UptimeSec: " + String(millis() / 1000) + "\r\n";
    out += "Mode: Passive nRF24 2.4GHz baseline compare\r\n";
    out += "RiskScore: " + String(rfDriftScore) + "\r\n";
    out += "RiskLabel: " + String(rfRiskLabel()) + "\r\n";
    out += "ChangedChannels: " + String(rfChangedChannels) + "\r\n";
    out += "PeakNrfChannel: " + String(rfDeltaPeakCh) + "\r\n";
    out += "PeakFreqMHz: " + String(nrfFreq(rfDeltaPeakCh)) + "\r\n";
    out += "PeakDelta: " + String(rfDeltaPeak) + "\r\n";
    int peakWifi = nrfToWifiCh(rfDeltaPeakCh);
    out += "PeakWifiChannel: " + String(peakWifi > 0 ? String(peakWifi) : String("--")) + "\r\n";
    out += "StrongestWifiDelta: CH" + String(strongestWifiDeltaChannel()) + "\r\n";
    out += "\r\nNRF_CH,FREQ_MHZ,BASELINE,LIVE,DELTA,WIFI_CH\r\n";
    for (int i = 0; i < SCAN_LIMIT; i++) {
        int wifiCh = nrfToWifiCh(i);
        out += String(i) + ",";
        out += String(nrfFreq(i)) + ",";
        out += String(rfBaseline[i]) + ",";
        out += String(samples[i]) + ",";
        out += String(rfDelta[i]) + ",";
        out += String(wifiCh > 0 ? String(wifiCh) : String("--")) + "\r\n";
    }
    return sdWriteTextFile("/RF_BASELINE.txt", out);
}

static void showRfSaveResult(bool ok) {
    drawRfFrame(ok ? "SAVE OK" : "SAVE ERROR");
    drawStringFit(20, 98, ok ? String("/RF_BASELINE.txt") : "No se pudo escribir SD",
                  ok ? TFT_CYAN : TFT_YELLOW, 280, 2);
    drawStringCustom(10, 222, "OK/BACK: RETURN", UI_ACCENT, 1);
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(80);
    flushNavInput();
}

void runRfBaseline() {
    drawScannerInitScreen();
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(40);
    flushNavInput();

    if (!initScannerRadios()) {
        drawScannerErrorScreen();
        cleanupScanner(true);
        return;
    }

    frameCount = 0;
    if (!captureRfBaseline(10)) {
        cleanupScanner(true);
        while (isEnterPressed() || isBackPressed()) delay(5);
        delay(60);
        flushNavInput();
        return;
    }

    playStartup();
    drawRfFrame("RF BASELINE");

    bool exitTool = false;
    while (!exitTool) {
        frameCount++;
        if (isBackPressed()) break;
        if (!doFullSweep()) break;
        computeRfDelta();
        drawRfBaselineLive();

        if (rfDeltaPeak >= 16) geigerAmbient(rfDeltaPeak);
        else ledcWriteTone(0, 0);

        NavAction action = readNavAction(120);
        if (action == NAV_BACK || isBackPressed()) {
            exitTool = true;
        } else if (action == NAV_UP || action == NAV_DOWN) {
            ledcWriteTone(0, 0);
            if (!captureRfBaseline(10)) {
                exitTool = true;
            } else {
                drawRfFrame("RF BASELINE");
                flushNavInput();
            }
        } else if (action == NAV_ENTER) {
            bool ok = exportRfBaselineReport();
            beep(ok ? 2400 : 900, 45);
            showRfSaveResult(ok);
            drawRfFrame("RF BASELINE");
        }

        delay(8);
    }

    playExit();
    cleanupScanner(true);
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(60);
    flushNavInput();
}

void runRadioScanner() {
    drawScannerInitScreen();
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(40);

    if (!initScannerRadios()) {
        drawScannerErrorScreen();
        cleanupScanner(true);
        return;
    }

    // Reset estado
    resetAnalyzerState(true);
    currentMode       = MODE_SPECTRUM;
    frameCount        = 0;

    drawCurrentFrame();
    drawCurrentHeader();
    drawCurrentFooter();
    playStartup();

    bool exitScanner = false;

    while (!exitScanner) {
        frameCount++;

        if (isBackPressed()) {
            exitScanner = true;
            break;
        }

        // Sweep completo
        if (!doFullSweep()) {
            exitScanner = true;
            break;
        }

        // Compute WiFi channel levels (usado por modo 3 y por el header)
        computeWifiChannels();

        // ── Render ───────────────────────────────────────────────────
        drawCurrentHeader();
        drawCurrentData();
        drawCurrentFooter();

        // ── Sonido ───────────────────────────────────────────────────
        if (currentMode == MODE_SPECTRUM) {
            geigerAmbient(peakValue);
        } else if (currentMode == MODE_CHANNEL) {
            ledcWriteTone(0, 0);
            if (bestWifiChannel != lastRecommendedCh) {
                if (lastRecommendedCh != -1) {
                    beep(2400, 40); delay(20);
                    beep(2400, 40);
                }
                lastRecommendedCh = bestWifiChannel;
            }
        } else {
            ledcWriteTone(0, 0);
        }

        // ── CONTROLES ────────────────────────────────────────────────
        NavAction action = readNavAction(120);
        if (action == NAV_UP) {
            ScanMode next = (ScanMode)((currentMode + 1) % 3);
            switchMode(next);
        }
        if (action == NAV_DOWN) {
            ScanMode prev = (ScanMode)((currentMode + 2) % 3);
            switchMode(prev);
        }
        if (action == NAV_BACK || isBackPressed()) {
            exitScanner = true;
        }
        if (action == NAV_ENTER) {
            unsigned long pressedAt = millis();
            while (navEnterPressed() && (millis() - pressedAt) < 450) {
                delay(10);
            }
            if (millis() - pressedAt >= 450) exitScanner = true;
        }
    }

    playExit();
    cleanupScanner(true);
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(60);
}
