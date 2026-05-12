#include "PepeDraw.h"
#include "Settings.h"
#include "Pins.h"
#include "NVSStore.h"
#include "WifiConfig.h"
#include "SoundUtils.h"

static int cursor = 0;
static const int MENU_ITEMS = 3;

// ═══════════════════════════════════════════════════════════════════════════
//  OLVIDAR RED WIFI · borra credenciales guardadas en NVS
// ═══════════════════════════════════════════════════════════════════════════
static void runForgetWifi() {
    // Esperar liberación de OK
    while (navEnterPressed()) delay(5);
    delay(100);

    // Caso 1: no hay red guardada
    if (!wifiConfigHasSaved()) {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, TFT_WHITE);
        drawStringCustom(30, 10, "WIFI CONFIG", TFT_WHITE, 3);
        tft.drawFastHLine(0, 45, 320, TFT_WHITE);

        drawStringCustom(40, 90, "SIN RED GUARDADA", UI_ACCENT, 2);
        drawStringCustom(40, 130, "No hay credenciales WiFi", TFT_WHITE, 1);
        drawStringCustom(40, 145, "guardadas en este momento.", TFT_WHITE, 1);
        drawStringCustom(10, 222, "OK: Volver", UI_ACCENT, 1);

        beep(1500, 60);

        while (!navEnterPressed()) delay(20);
        beep(1800, 40);
        while (navEnterPressed()) delay(5);
        delay(100);
        return;
    }

    // Caso 2: hay red guardada → confirmar
    String savedSSID = wifiConfigGetSavedSSID();

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawStringCustom(40, 12, "OLVIDAR WIFI", TFT_RED, 3);
    tft.drawFastHLine(0, 50, 320, TFT_RED);

    drawStringCustom(20, 70, "Red guardada:", TFT_WHITE, 1);

    if (getTextWidth(savedSSID, 2) <= 280) {
        drawStringCustom(20, 90, savedSSID, UI_SELECT, 2);
    } else {
        drawStringFit(20, 95, savedSSID, UI_SELECT, 280, 1);
    }

    drawStringCustom(20, 130, "Eliminar credenciales?", TFT_WHITE, 1);
    drawStringCustom(20, 144, "La proxima vez que uses una", UI_ACCENT, 1);
    drawStringCustom(20, 156, "herramienta con WiFi tendras", UI_ACCENT, 1);
    drawStringCustom(20, 168, "que escoger una red de nuevo.", UI_ACCENT, 1);

    tft.drawFastHLine(0, 210, 320, TFT_RED);
    drawStringCustom(10, 220, "OK: SI BORRAR   UP/DN: CANCELAR", UI_ACCENT, 1);

    while (true) {
        if (navEnterPressed()) {
            beep(1200, 80);
            while (navEnterPressed()) delay(5);
            delay(100);

            // Borrar credenciales
            wifiConfigForget();

            // Pantalla de confirmación
            tft.fillScreen(TFT_BLACK);
            tft.drawRect(0, 0, 320, 240, TFT_WHITE);
            drawStringCustom(40, 90, "RED OLVIDADA", TFT_GREEN, 3);
            drawStringCustom(40, 140, "Credenciales eliminadas.", TFT_WHITE, 1);

            beep(2400, 50); delay(30);
            beep(3000, 80);
            delay(1500);
            return;
        }
        if (navUpPressed() || navDownPressed()) {
            beep(2000, 40);
            while (navUpPressed() || navDownPressed())
                delay(5);
            delay(100);
            return;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MENU SETTINGS PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════════

void drawSettings() {
    tft.fillScreen(TFT_BLACK);

    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringCustom(30, 10, "SETTINGS", TFT_WHITE, 3);
    tft.drawFastHLine(0, 45, 320, TFT_WHITE);

    String soundStr = soundEnabled ? "ON" : "OFF";

    for (int i = 0; i < MENU_ITEMS; i++) {
        int y = 60 + (i * 38);

        if (i == cursor) {
            tft.fillRect(10, y - 5, 300, 30, TFT_WHITE);
        }

        uint16_t textColor = (i == cursor) ? TFT_BLACK : TFT_WHITE;

        if (i == 0) {
            drawStringCustom(20, y, "SOUND: " + soundStr, textColor, 2);
        }
        else if (i == 1) {
            drawStringCustom(20, y, "VOLUME: " + String(soundVolume),
                             textColor, 2);
        }
        else if (i == 2) {
            drawStringCustom(20, y, "FORGET WIFI", textColor, 2);
        }
    }

    tft.drawFastHLine(0, 210, 320, TFT_WHITE);
    drawStringCustom(10, 220, "OK: SELECT   OK(HOLD): BACK", UI_ACCENT, 1);
}

void runSettings() {

    cursor = 0;

    // Evitar doble OK
    while (navEnterPressed());
    delay(150);

    bool exitMenu = false;

    drawSettings();

    while (!exitMenu) {

        if (navDownPressed()) {
            cursor = (cursor + 1) % MENU_ITEMS;
            drawSettings();
            delay(200);
        }

        if (navUpPressed()) {
            cursor = (cursor - 1 + MENU_ITEMS) % MENU_ITEMS;
            drawSettings();
            delay(200);
        }

        if (navEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            if (held) {
                exitMenu = true;
                beep(1000, 40);
                delay(120);
                continue;
            }

            if (cursor == 0) {
                soundEnabled = !soundEnabled;
                nvsSetBool("sound_on", soundEnabled);
            }
            else if (cursor == 1) {
                soundVolume++;
                if (soundVolume > 5) soundVolume = 1;
                nvsSetInt("sound_vol", soundVolume);
            }
            else if (cursor == 2) {
                // Esperar liberación antes de entrar a la sub-pantalla
                while (navEnterPressed());
                delay(100);
                runForgetWifi();
            }
            drawSettings();
            delay(150);
        }

        delay(10);
    }
}
