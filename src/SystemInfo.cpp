#include "SystemInfo.h"
#include "DisplayTFT.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include "PepeDraw.h"
#include "Pins.h"
#include "Input.h"
#include "SoundUtils.h"
#include "NVSStore.h"

extern DisplayTFT tft;

// ═══════════════════════════════════════════════════════════════════════════
//  LECTURA DE TEMPERATURA INTERNA
//  El ESP32 tiene un sensor de temperatura interno accesible vía ROM
//  function (no documentada oficialmente pero estable y ampliamente usada).
// ═══════════════════════════════════════════════════════════════════════════
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read() __attribute__((weak));
#ifdef __cplusplus
}
#endif

static float readChipTemperatureC() {
    if (temprature_sens_read == nullptr) return 0;
    // Lectura raw (resultado en °F aproximado, convertimos a °C)
    uint8_t raw = temprature_sens_read();
    float fahrenheit = (float)raw;
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

// ═══════════════════════════════════════════════════════════════════════════
//  FORMATEADORES
// ═══════════════════════════════════════════════════════════════════════════

// Uptime formateado HH:MM:SS
static String formatUptime(unsigned long ms) {
    unsigned long totalSec = ms / 1000;
    unsigned long hours = totalSec / 3600;
    unsigned long mins  = (totalSec % 3600) / 60;
    unsigned long secs  = totalSec % 60;

    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hours, mins, secs);
    return String(buf);
}

// Formatea un MAC address en AA:BB:CC:DD:EE:FF
static String formatMAC(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

// Nombre del chip según esp_chip_info_t
static const char* chipModelName(esp_chip_model_t model) {
    switch (model) {
        case CHIP_ESP32:    return "ESP32";
        case CHIP_ESP32S2:  return "ESP32-S2";
        case CHIP_ESP32S3:  return "ESP32-S3";
        case CHIP_ESP32C3:  return "ESP32-C3";
        case CHIP_ESP32H2:  return "ESP32-H2";
        default:            return "Unknown";
    }
}

// Formatea KB con 0 decimales
static String formatKB(uint32_t bytes) {
    return String(bytes / 1024) + " KB";
}

static String formatMB(uint32_t bytes) {
    return String(bytes / (1024 * 1024)) + " MB";
}

// ═══════════════════════════════════════════════════════════════════════════
//  DIBUJO
// ═══════════════════════════════════════════════════════════════════════════

// Dibuja el marco estático (título, separadores, section headers, footer).
// Solo se llama una vez al entrar.
static void drawStaticLayout() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    // Header
    drawStringBig(10, 8, "SYSTEM INFO", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // Section headers (estáticos)
    drawStringCustom(10, 38,  "FIRMWARE",  UI_SELECT, 1);
    drawStringCustom(10, 82,  "HARDWARE",  UI_SELECT, 1);
    drawStringCustom(10, 158, "RUNTIME",   UI_SELECT, 1);

    // Footer
    tft.drawFastHLine(0, 218, 320, UI_ACCENT);
    drawStringCustom(10, 225, "OK (HOLD): BACK TO MENU", UI_ACCENT, 1);
}

// Dibuja la información estática (no cambia en runtime)
static void drawStaticInfo() {
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint8_t macWifi[6];
    uint8_t macBT[6];
    esp_read_mac(macWifi, ESP_MAC_WIFI_STA);
    esp_read_mac(macBT,   ESP_MAC_BT);

    // ── FIRMWARE ────────────────────────────────────────────────────────
    drawStringCustom(20, 50,
        "Version: " + String(FW_NAME) + " " + String(FW_VERSION),
        UI_MAIN, 1);
    drawStringCustom(20, 62,
        "Built:   " + String(__DATE__),
        UI_ACCENT, 1);

    // ── HARDWARE ────────────────────────────────────────────────────────
    drawStringCustom(20, 94,
        "Chip:    " + String(chipModelName(chip.model)) +
        " rev" + String(chip.revision),
        UI_MAIN, 1);

    drawStringCustom(20, 106,
        "Cores:   " + String(chip.cores) +
        "  |  " + String(ESP.getCpuFreqMHz()) + " MHz",
        UI_MAIN, 1);

    drawStringCustom(20, 118,
        "Flash:   " + formatMB(ESP.getFlashChipSize()),
        UI_MAIN, 1);

    drawStringCustom(20, 130,
        "MAC WiFi: " + formatMAC(macWifi),
        UI_MAIN, 1);

    drawStringCustom(20, 142,
        "MAC BT:   " + formatMAC(macBT),
        UI_MAIN, 1);
}

// Redibuja solo los valores dinámicos (se llama periódicamente)
static void drawDynamicInfo(unsigned long sessionStartMs) {
    // Valores a mostrar
    unsigned long uptimeMs   = millis() - sessionStartMs;
    unsigned long bootCount  = nvsGetULong("boot_cnt", 0);
    uint32_t heapFree        = ESP.getFreeHeap();
    uint32_t heapTotal       = ESP.getHeapSize();
    float    tempC           = readChipTemperatureC();

    // Sanear temperatura (a veces el sensor devuelve valores absurdos al inicio)
    if (tempC < 0 || tempC > 125) tempC = 0;

    // Área dinámica: y 170-212 (borrar antes de redibujar)
    tft.fillRect(18, 170, 294, 44, TFT_BLACK);

    drawStringCustom(20, 170,
        "Boot #:   " + String(bootCount),
        UI_MAIN, 1);

    drawStringCustom(20, 182,
        "Uptime:   " + formatUptime(uptimeMs),
        UI_MAIN, 1);

    drawStringCustom(20, 194,
        "Heap:     " + formatKB(heapFree) + " / " + formatKB(heapTotal),
        UI_MAIN, 1);

    if (tempC > 0) {
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%.1f C", tempC);
        drawStringCustom(20, 206, "Temp:     " + String(tbuf), UI_MAIN, 1);
    } else {
        drawStringCustom(20, 206, "Temp:     --", UI_ACCENT, 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN · SYSTEM INFO
// ═══════════════════════════════════════════════════════════════════════════
void runSystemInfo() {

    // Esperar a que se libere OK del menú anterior
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(100);

    beep(1800, 30);

    unsigned long sessionStart = millis();
    drawStaticLayout();
    drawStaticInfo();
    drawDynamicInfo(sessionStart);

    unsigned long lastRefresh = millis();
    const unsigned long REFRESH_MS = 500;

    bool exitScreen = false;
    unsigned long okPressStart = 0;
    bool okHeld = false;

    while (!exitScreen) {
        // Refresh periódico de valores dinámicos
        if (millis() - lastRefresh > REFRESH_MS) {
            drawDynamicInfo(sessionStart);
            lastRefresh = millis();
        }

        // Detectar hold de OK (~300ms) para salir
        if (isBackPressed()) {
            exitScreen = true;
        } else if (isEnterPressed()) {
            if (!okHeld) {
                okPressStart = millis();
                okHeld = true;
            } else if (millis() - okPressStart > 300) {
                exitScreen = true;
            }
        } else {
            okHeld = false;
        }

        delay(10);
    }

    beep(1400, 30);
    delay(20);
    beep(1800, 40);

    // Esperar liberación
    while (isEnterPressed() || isBackPressed()) delay(5);
    delay(100);
}
