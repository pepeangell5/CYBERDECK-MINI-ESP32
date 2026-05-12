#include "RadioJammer.h"
#include <RF24.h>
#include <SPI.h>
#include "Pins.h"
#include "Input.h"

extern DisplayTFT tft;

static RF24 radioJam1(NRF1_CE_PIN, NRF1_CSN_PIN);
static RF24 radioJam2(NRF2_CE_PIN, NRF2_CSN_PIN);
static RF24* jamRadios[] = { &radioJam1, &radioJam2 };
static bool jamRadioOk[] = { false, false };
static const uint8_t jamRadioCount = sizeof(jamRadios) / sizeof(jamRadios[0]);

// ── Canal WiFi seleccionado (1-13) ────────────────────────────────────────────
static int jamChannel = 6;

// ── Tabla: canal WiFi → offset NRF24 (base 2400 MHz) ─────────────────────────
static const uint8_t wifiToNrf[14] = {
    0,  12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72
};

// ── Payload de ruido máximo 32 bytes ─────────────────────────────────────────
static const uint8_t noise_payload[32] = {
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA,
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA
};

// ── Solo los 14 canales WiFi 2.4GHz (centros exactos en offset NRF24) ─────────
// Menos canales = más tiempo por canal = mayor saturación efectiva
static const uint8_t sweep_list[] = {
    12,  // WiFi CH1  → 2412 MHz
    17,  // WiFi CH2  → 2417 MHz
    22,  // WiFi CH3  → 2422 MHz
    27,  // WiFi CH4  → 2427 MHz
    32,  // WiFi CH5  → 2432 MHz
    37,  // WiFi CH6  → 2437 MHz
    42,  // WiFi CH7  → 2442 MHz
    47,  // WiFi CH8  → 2447 MHz
    52,  // WiFi CH9  → 2452 MHz
    57,  // WiFi CH10 → 2457 MHz
    62,  // WiFi CH11 → 2462 MHz
    67,  // WiFi CH12 → 2467 MHz
    //72,  // WiFi CH13 → 2472 MHz
    //84   // WiFi CH14 → 2484 MHz (Japón)
};
static const int sweep_total = sizeof(sweep_list);

// ─────────────────────────────────────────────────────────────────────────────
// Inicializa el NRF24 usando el bus SPI compartido con la TFT
// ─────────────────────────────────────────────────────────────────────────────
static void configureJamRadio(RF24& radio) {
    radio.powerUp();
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.setAutoAck(false);
    radio.setRetries(0, 0);
    radio.setAddressWidth(3);
    radio.openWritingPipe((uint8_t*)"JAM");
    radio.stopListening();
}

static bool initRadio() {
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
    delay(20);

    jamRadioOk[0] = radioJam1.begin();
    if (jamRadioOk[0]) configureJamRadio(radioJam1);

    jamRadioOk[1] = radioJam2.begin();
    if (jamRadioOk[1]) configureJamRadio(radioJam2);

    Serial.printf("[RadioJammer] NRF1 CE:%d CSN:%d -> %s\n",
                  NRF1_CE_PIN, NRF1_CSN_PIN, jamRadioOk[0] ? "OK" : "FAIL");
    Serial.printf("[RadioJammer] NRF2 CE:%d CSN:%d -> %s\n",
                  NRF2_CE_PIN, NRF2_CSN_PIN, jamRadioOk[1] ? "OK" : "FAIL");
    Serial.printf("[RadioJammer] SPI SCK:%d MISO:%d MOSI:%d TFT_CS:%d\n",
                  SCK_PIN, MISO_PIN, MOSI_PIN, TFT_CS_PIN);

    return jamRadioOk[0] || jamRadioOk[1];
}

static int activeRadioCount() {
    return (jamRadioOk[0] ? 1 : 0) + (jamRadioOk[1] ? 1 : 0);
}

static void prepareJammerDisplay() {
    digitalWrite(NRF1_CE_PIN, LOW);
    digitalWrite(NRF2_CE_PIN, LOW);
    digitalWrite(NRF1_CSN_PIN, HIGH);
    digitalWrite(NRF2_CSN_PIN, HIGH);
    digitalWrite(TFT_CS_PIN, HIGH);
    delayMicroseconds(80);
}

static void waitJammerControlsReleased() {
    while (isEnterPressed() || isBackPressed()) {
        readNavAction(0);
        delay(5);
    }
    readNavAction(0);
    delay(90);
}

static void cleanupJammer(bool clearScreen) {
    for (uint8_t r = 0; r < jamRadioCount; r++) {
        if (jamRadioOk[r]) {
            jamRadios[r]->stopListening();
            jamRadios[r]->powerDown();
        }
    }

    digitalWrite(NRF1_CE_PIN, LOW);
    digitalWrite(NRF2_CE_PIN, LOW);
    digitalWrite(NRF1_CSN_PIN, HIGH);
    digitalWrite(NRF2_CSN_PIN, HIGH);
    digitalWrite(TFT_CS_PIN, HIGH);

    if (clearScreen) {
        prepareJammerDisplay();
        tft.fillScreen(TFT_BLACK);
        delay(25);
    }
}

static void drawJammerInitScreen() {
    prepareJammerDisplay();
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringBig(22, 78, "JAMMER INIT", TFT_WHITE, 2);
    drawStringCustom(44, 124, "Preparando NRF24...", UI_ACCENT, 1);
    drawStringCustom(32, 146, "BACK: cancelar", UI_ACCENT, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Animación de barras
// ─────────────────────────────────────────────────────────────────────────────
static void drawBars() {
    int baseY = 205;
    int areaH = 30;
    tft.fillRect(10, baseY - areaH, 300, areaH, TFT_BLACK);
    for (int x = 15; x < 310; x += 4) {
        int h = random(3, areaH);
        tft.drawFastVLine(x, baseY - h, h, h > areaH / 2 ? TFT_RED : TFT_YELLOW);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NÚCLEOS DE ATAQUE
//
//  attackTurbo()  — Máxima densidad en un solo canal.
//                   Todo el tiempo del radio concentrado en el canal exacto.
//                   Más efectivo para tumbar un canal WiFi específico.
//
//  attackWide()   — Canal central ±2 (5 canales NRF = ~10 MHz de ancho).
//                   Balance entre cobertura y densidad.
//                   Útil si el AP salta entre canales adyacentes.
//
//  attackSweep()  — Recorre toda la lista de canales WiFi + BT.
//                   Menor densidad por canal pero cobertura total.
// ─────────────────────────────────────────────────────────────────────────────

// Turbo: 500 paquetes seguidos en un solo canal — máxima saturación
static void transmitBurst(RF24& radio, uint8_t nrfChannel, int packets) {
    radio.setChannel(nrfChannel);
    for (int i = 0; i < packets; i++)
        radio.startWrite(noise_payload, 32, true);
}

static void transmitBurstAll(uint8_t nrfChannel, int packetsPerRadio) {
    for (uint8_t r = 0; r < jamRadioCount; r++) {
        if (!jamRadioOk[r]) continue;
        transmitBurst(*jamRadios[r], nrfChannel, packetsPerRadio);
    }
}

static void attackTurbo(uint8_t nrfChannel) {
    transmitBurstAll(nrfChannel, 500);
}

// Wide: ±2 canales NRF alrededor del centro (5 canales total, ~10 MHz BW)
// Cada canal recibe 100 paquetes → densidad alta con algo de cobertura lateral
static void attackWide(uint8_t center) {
    for (int offset = -2; offset <= 2; offset++) {
        int ch = (int)center + offset;
        if (ch < 0 || ch > 125) continue;
        transmitBurstAll((uint8_t)ch, 100);
    }
}

// Sweep: ataca cada canal con BURST_PER_CH paquetes antes de pasar al siguiente.
// Recorre un canal completo por llamada para que el analizador lo detecte
// y el AP no pueda usarlo durante el tiempo de ataque en ese canal.
#define SWEEP_BURST_PER_CH 80
static void attackSweepStep(int& idx) {
    for (uint8_t r = 0; r < jamRadioCount; r++) {
        if (!jamRadioOk[r]) continue;
        transmitBurst(*jamRadios[r], sweep_list[idx], SWEEP_BURST_PER_CH);
        idx = (idx + 1) % sweep_total;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MODO 1: Canal WiFi fijo — tres sub-modos de ataque seleccionables
//
// Navegación (sin atacar):
//   UP/DOWN   → cambia canal WiFi
//   OK        → si foco en canal → cicla modo de ataque (OFF→TURBO→WIDE→OFF)
//               si foco en BACK  → vuelve al menú del jammer
//   DOWN en canal 1 → mueve foco a BACK
//   UP en BACK      → mueve foco a canal
// ─────────────────────────────────────────────────────────────────────────────
static void runChannelJammer() {
    // attackMode: 0=OFF, 1=TURBO (concentrado), 2=WIDE (±2 canales)
    int     attackMode = 0;
    bool    exitMode   = false;
    const char* modeLabels[] = { "OFF", "TURBO", "WIDE" };
    const uint16_t modeColors[] = { TFT_GREEN, TFT_RED, TFT_ORANGE };

    auto redraw = [&]() {
        prepareJammerDisplay();
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, TFT_WHITE);

        // Cabecera — color según modo activo
        uint16_t hdrBg = (attackMode == 0) ? TFT_WHITE :
                         (attackMode == 1) ? TFT_RED : 0xFBE0; // naranja oscuro
        tft.fillRect(1, 1, 318, 42, hdrBg);

        if (attackMode == 0)
            drawStringCustom(10, 10, "CANAL FIJO", TFT_BLACK, 3);
        else if (attackMode == 1)
            drawStringCustom(10, 10, "TURBO JAM!", TFT_WHITE, 3);
        else
            drawStringCustom(10, 10, "WIDE  JAM!", TFT_WHITE, 3);

        // Canal seleccionado
        uint16_t chBg = (attackMode == 0) ? TFT_WHITE : TFT_BLACK;
        uint16_t chFg = (attackMode == 0) ? TFT_BLACK : TFT_YELLOW;
        tft.fillRect(5, 50, 310, 28, chBg);
        drawStringCustom(10, 56,
            "CH " + String(jamChannel) + "  " +
            String(2412 + (jamChannel - 1) * 5) + " MHz",
            chFg, 2);

        // Modo de ataque actual
        drawStringCustom(10, 88, "MODO: ", TFT_WHITE, 2);
        drawStringCustom(80, 88, modeLabels[attackMode], modeColors[attackMode], 2);

        // Info según modo
        if (attackMode == 0) {
            drawStringCustom(10, 115, "OK: TURBO → WIDE → OFF", UI_ACCENT, 1);
        } else if (attackMode == 1) {
            drawStringCustom(10, 115, "500 pkt/iter canal exacto", TFT_RED, 1);
            drawStringCustom(10, 132, "Maxima densidad. 1 canal.", TFT_RED, 1);
        } else {
            drawStringCustom(10, 115, "100 pkt x 5 canales (+/-2)", TFT_ORANGE, 1);
            drawStringCustom(10, 132, "Cubre ~10 MHz de ancho.", TFT_ORANGE, 1);
        }

        tft.drawFastHLine(0, 215, 320, TFT_WHITE);
        if (attackMode == 0)
            drawStringCustom(5, 220, "OK:MODO  BACK/OK(H):SALIR", UI_ACCENT, 1);
        else
            drawStringCustom(5, 220, "UP/DN:CANAL OK:MODO BACK:SALIR", UI_ACCENT, 1);
    };

    redraw();

    while (!exitMode) {
        NavAction action = readNavAction(90);

        // ── UP ──────────────────────────────────────────────────────────────
        if (action == NAV_UP) {
            jamChannel = (jamChannel == 13) ? 1 : jamChannel + 1;
            redraw();
        }

        // ── DOWN ─────────────────────────────────────────────────────────────
        if (action == NAV_DOWN) {
            jamChannel = (jamChannel == 1) ? 13 : jamChannel - 1;
            redraw();
        }

        // ── OK ───────────────────────────────────────────────────────────────
        if (action == NAV_BACK) {
            exitMode = true;
            break;
        }

        if (action == NAV_ENTER) {
            bool held = waitOkReleaseWasLong();
            if (held) {
                exitMode = true;
                break;
            }
            attackMode = (attackMode + 1) % 3;
            redraw();
        }

        // ── ATAQUE ───────────────────────────────────────────────────────────
        if (attackMode == 1) {
            attackTurbo(wifiToNrf[jamChannel]);
            drawBars();
        } else if (attackMode == 2) {
            attackWide(wifiToNrf[jamChannel]);
            drawBars();
        }
    }
    waitJammerControlsReleased();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODO 2: Barrido total WiFi + Bluetooth
// ─────────────────────────────────────────────────────────────────────────────
static void runSweepJammer() {
    bool isAttacking = false;
    bool exitMode    = false;
    int  sweepIdx    = 0;
    int  animCtr     = 0;

    auto redraw = [&]() {
        prepareJammerDisplay();
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, TFT_WHITE);

        tft.fillRect(1, 1, 318, 42, isAttacking ? TFT_RED : TFT_WHITE);
        drawStringCustom(10, 10, isAttacking ? "BARRIDO ACTIVO" : "BARRIDO TOTAL",
                         isAttacking ? TFT_WHITE : TFT_BLACK, 2);

        if (isAttacking) {
            drawStringCustom(10, 55, "WiFi CH1-13 + Bluetooth", TFT_RED, 2);
            drawStringCustom(10, 80, String(sweep_total) + " canales en bucle", TFT_YELLOW, 2);
            drawStringCustom(10, 102,
                "RADIOS: " + String((int)jamRadioOk[0] + (int)jamRadioOk[1]) + "/2",
                TFT_CYAN, 1);
        } else {
            drawStringCustom(10, 55, "WiFi 2.4GHz + Bluetooth", TFT_WHITE, 2);
            drawStringCustom(10, 80, "ESTADO: LISTO", TFT_GREEN, 2);

            tft.fillRect(5, 115, 140, 30, TFT_WHITE);
            tft.drawRect(5, 115, 140, 30, TFT_WHITE);
            drawStringCustom(14, 123, ">> INICIAR", TFT_BLACK, 2);
        }

        tft.drawFastHLine(0, 215, 320, TFT_WHITE);
        drawStringCustom(5, 220,
            isAttacking ? "OK:PARAR  BACK:SALIR" : "OK:INICIAR  BACK/OK(H):SALIR",
            UI_ACCENT, 1);
    };

    redraw();

    while (!exitMode) {
        if (isAttacking) {
            attackSweepStep(sweepIdx);

            // Redibujar barra y canal cada 5 canales para feedback visual fluido
            if (++animCtr >= 5) {
                animCtr = 0;
                drawBars();
                // Mostrar canal NRF actual en tiempo real
                tft.fillRect(10, 155, 250, 18, TFT_BLACK);
                drawStringCustom(10, 157,
                    "NRF CH: " + String(sweep_list[sweepIdx]) +
                    "  (" + String(sweepIdx + 1) + "/" + String(sweep_total) + ")",
                    TFT_CYAN, 2);
            }
            NavAction action = readNavAction(90);
            if (action == NAV_BACK) {
                exitMode = true;
            } else if (action == NAV_ENTER) {
                isAttacking = false;
                redraw();
            }
        } else {
            NavAction action = readNavAction(90);
            if (action == NAV_BACK) {
                exitMode = true;
            } else if (action == NAV_ENTER) {
                bool held = waitOkReleaseWasLong();
                if (held) {
                    exitMode = true;
                } else {
                    isAttacking = true;
                    sweepIdx = 0;
                    animCtr  = 0;
                    redraw();
                }
            }
            delay(10);
        }
    }
    waitJammerControlsReleased();
}

// ─────────────────────────────────────────────────────────────────────────────
// Menú principal del jammer (0=Canal Fijo, 1=Barrido, 2=Back)
// ─────────────────────────────────────────────────────────────────────────────
static void drawDiagnostics() {
    prepareJammerDisplay();
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringCustom(16, 10, "NRF DIAGNOSTICO", TFT_WHITE, 2);
    tft.drawFastHLine(0, 38, 320, TFT_WHITE);

    uint16_t nrf1Color = jamRadioOk[0] ? TFT_GREEN : TFT_RED;
    uint16_t nrf2Color = jamRadioOk[1] ? TFT_GREEN : TFT_RED;

    drawStringCustom(12, 52, "NRF1:", TFT_WHITE, 2);
    drawStringCustom(80, 52, jamRadioOk[0] ? "OK" : "FAIL", nrf1Color, 2);
    drawStringCustom(140, 55,
        "CE " + String(NRF1_CE_PIN) + "  CSN " + String(NRF1_CSN_PIN),
        UI_ACCENT, 1);

    drawStringCustom(12, 82, "NRF2:", TFT_WHITE, 2);
    drawStringCustom(80, 82, jamRadioOk[1] ? "OK" : "FAIL", nrf2Color, 2);
    drawStringCustom(140, 85,
        "CE " + String(NRF2_CE_PIN) + "  CSN " + String(NRF2_CSN_PIN),
        UI_ACCENT, 1);

    drawStringCustom(12, 118, "SPI:", TFT_WHITE, 2);
    drawStringCustom(80, 121,
        "SCK " + String(SCK_PIN) + "  MISO " + String(MISO_PIN) +
        "  MOSI " + String(MOSI_PIN),
        UI_ACCENT, 1);
    drawStringCustom(80, 138,
        "TFT CS " + String(TFT_CS_PIN) + " en HIGH al iniciar",
        UI_ACCENT, 1);

    int nrfCh = wifiToNrf[jamChannel];
    drawStringCustom(12, 168, "CANAL:", TFT_WHITE, 2);
    drawStringCustom(95, 171,
        "WiFi " + String(jamChannel) + " -> NRF " + String(nrfCh) +
        " (" + String(2400 + nrfCh) + " MHz)",
        TFT_CYAN, 1);

    drawStringCustom(12, 198,
        "RADIOS ACTIVOS: " + String(activeRadioCount()) + "/2",
        activeRadioCount() == 2 ? TFT_GREEN : TFT_YELLOW, 1);
    drawStringCustom(12, 220, "OK/BACK: VOLVER", UI_ACCENT, 1);
}

static void runDiagnostics() {
    drawDiagnostics();
    Serial.println("[RadioJammer] Diagnostic screen opened");
    while (!isEnterPressed() && !isBackPressed()) delay(10);
    waitJammerControlsReleased();
}

static void drawModeMenu(int sel) {
    prepareJammerDisplay();
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_WHITE);
    drawStringCustom(20, 10, "RADIO JAMMER", TFT_WHITE, 3);
    tft.drawFastHLine(0, 45, 320, TFT_WHITE);

    tft.fillRect(10, 50,  300, 36, sel == 0 ? TFT_WHITE : TFT_BLACK);
    tft.drawRect( 10, 50,  300, 36, TFT_WHITE);
    drawStringCustom(20, 58,  "1. CANAL FIJO", sel == 0 ? TFT_BLACK : TFT_WHITE, 2);

    tft.fillRect(10, 92, 300, 36, sel == 1 ? TFT_WHITE : TFT_BLACK);
    tft.drawRect( 10, 92, 300, 36, TFT_WHITE);
    drawStringCustom(20, 100, "2. BARRIDO TOTAL", sel == 1 ? TFT_BLACK : TFT_WHITE, 2);

    tft.fillRect(10, 134, 300, 36, sel == 2 ? TFT_WHITE : TFT_BLACK);
    tft.drawRect( 10, 134, 300, 36, TFT_WHITE);
    drawStringCustom(20, 142, "3. DIAGNOSTICO NRF", sel == 2 ? TFT_BLACK : TFT_WHITE, 2);

    tft.drawFastHLine(0, 212, 320, TFT_WHITE);
    drawStringCustom(10, 218, "OK:ENTRAR  BACK/OK(H):SALIR", UI_ACCENT, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Punto de entrada público
// ─────────────────────────────────────────────────────────────────────────────
void runRadioJammer() {
    drawJammerInitScreen();
    waitJammerControlsReleased();

    if (!initRadio()) {
        prepareJammerDisplay();
        tft.fillScreen(TFT_BLACK);
        drawStringCustom(20, 90,  "NRF24 ERROR", TFT_RED, 3);
        drawStringCustom(10, 130, "Revisa conexion SPI", TFT_WHITE, 2);
        drawStringCustom(10, 145, "NRF1 CE:" + String(NRF1_CE_PIN) + " CSN:" + String(NRF1_CSN_PIN), UI_ACCENT, 1);
        drawStringCustom(10, 162, "NRF2 CE:" + String(NRF2_CE_PIN) + " CSN:" + String(NRF2_CSN_PIN), UI_ACCENT, 1);
        drawStringCustom(10, 179, "SPI " + String(SCK_PIN) + "/" + String(MISO_PIN) + "/" + String(MOSI_PIN), UI_ACCENT, 1);
        drawStringCustom(10, 220, "OK/BACK: VOLVER", UI_ACCENT, 1);
        while (!isEnterPressed() && !isBackPressed()) delay(10);
        waitJammerControlsReleased();
        cleanupJammer(true);
        return;
    }

    prepareJammerDisplay();

    bool exitJammer = false;

    while (!exitJammer) {
        int  menuSel  = 0;
        bool menuDone = false;

        drawModeMenu(menuSel);
        delay(30);
        drawModeMenu(menuSel);

        while (!menuDone) {
            NavAction action = readNavAction(95);

            if (action == NAV_UP) {
                menuSel = (menuSel == 0) ? 2 : menuSel - 1;
                drawModeMenu(menuSel);
            }
            if (action == NAV_DOWN) {
                menuSel = (menuSel == 2) ? 0 : menuSel + 1;
                drawModeMenu(menuSel);
            }
            if (action == NAV_BACK) {
                exitJammer = true;
                menuDone = true;
                waitJammerControlsReleased();
                break;
            }
            if (action == NAV_ENTER) {
                if (waitOkReleaseWasLong()) {
                    exitJammer = true;
                }
                menuDone = true;
            }
            delay(10);
        }

        if      (exitJammer) break;
        else if (menuSel == 2) runDiagnostics();
        else if (menuSel == 0) runChannelJammer();
        else                   runSweepJammer();
    }

    cleanupJammer(true);
    waitJammerControlsReleased();
}
