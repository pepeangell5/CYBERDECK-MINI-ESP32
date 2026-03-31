#include "menu_icons.h"
#include <math.h>

// Definimos el color 
#ifndef MATRIX_GREEN
#define MATRIX_GREEN 0x07E0
#endif

void drawGlifoIcon(LGFX &tft, int cx, int cy, int index) {
    tft.startWrite();
    tft.fillCircle(cx, cy, 38, TFT_BLACK); 
    tft.drawCircle(cx, cy, 35, MATRIX_GREEN); 

    tft.setTextColor(MATRIX_GREEN);
    
    switch(index) {
        case 0: // WIFI SCANNER -> Radar
            tft.drawCircle(cx, cy, 25, MATRIX_GREEN);
            tft.drawCircle(cx, cy, 15, MATRIX_GREEN);
            tft.fillCircle(cx, cy, 5, MATRIX_GREEN);
            break;

        case 1: // BEACON SPAMMER -> Alerta
            tft.drawTriangle(cx, cy - 25, cx - 22, cy + 15, cx + 22, cy + 15, MATRIX_GREEN);
            tft.setTextSize(3);
            tft.setCursor(cx - 8, cy - 8); tft.print("!");
            break;

        case 2: // EVIL PORTAL -> Portal de entrada
            tft.drawRect(cx - 20, cy - 20, 40, 40, MATRIX_GREEN);
            tft.drawFastHLine(cx - 30, cy, 30, MATRIX_GREEN);
            tft.drawFastVLine(cx, cy - 5, 10, MATRIX_GREEN);
            break;

        case 3: // PACKET MONITOR -> Histograma
            tft.drawRect(cx - 20, cy - 15, 40, 30, MATRIX_GREEN);
            tft.drawFastVLine(cx - 10, cy, 10, MATRIX_GREEN);
            tft.drawFastVLine(cx, cy - 10, 20, MATRIX_GREEN);
            tft.drawFastVLine(cx + 10, cy - 5, 15, MATRIX_GREEN);
            break;

        case 4: // GPS TRACKER -> Mira telescópica
            tft.drawCircle(cx, cy, 20, MATRIX_GREEN);
            tft.drawFastHLine(cx - 30, cy, 60, MATRIX_GREEN);
            tft.drawFastVLine(cx, cy - 30, 60, MATRIX_GREEN);
            break;

        case 5: // NRF ANALYZER -> Antenas
            tft.drawLine(cx - 15, cy + 20, cx - 5, cy - 10, MATRIX_GREEN);
            tft.drawLine(cx + 15, cy + 20, cx + 5, cy - 10, MATRIX_GREEN);
            tft.drawFastHLine(cx - 25, cy + 20, 50, MATRIX_GREEN);
            break;

        case 6: // NRF JAMMER -> Pinza de ataque
            tft.drawLine(cx - 20, cy - 20, cx + 20, cy + 20, MATRIX_GREEN);
            tft.drawLine(cx + 20, cy - 20, cx - 20, cy + 20, MATRIX_GREEN);
            tft.drawRect(cx - 15, cy - 15, 30, 30, MATRIX_GREEN);
            break;

        case 7: // NRF SPECTRUM -> Barras de Frecuencia
            tft.drawFastHLine(cx - 20, cy + 15, 40, MATRIX_GREEN); 
            tft.fillRect(cx - 15, cy + 5,  6, 10, MATRIX_GREEN);  
            tft.fillRect(cx - 5,  cy - 10, 6, 25, MATRIX_GREEN);  
            tft.fillRect(cx + 5,  cy - 2,  6, 17, MATRIX_GREEN);  
            tft.fillRect(cx + 15, cy + 8,  6, 7,  MATRIX_GREEN);  
            break;

        case 8: // FILE EXPLORER -> Tarjeta MicroSD
            tft.drawRect(cx - 15, cy - 20, 30, 40, MATRIX_GREEN);
            tft.drawLine(cx - 15, cy - 15, cx - 10, cy - 20, MATRIX_GREEN); 
            tft.drawRect(cx - 10, cy - 15, 4, 10, MATRIX_GREEN); 
            tft.drawRect(cx, cy - 15, 4, 10, MATRIX_GREEN);
            tft.drawRect(cx + 6, cy - 15, 4, 10, MATRIX_GREEN);
            break;

        case 9: // VIEW LOGS -> Carpeta
            tft.drawRect(cx - 20, cy - 15, 40, 35, MATRIX_GREEN);
            tft.fillRect(cx - 20, cy - 20, 15, 5, MATRIX_GREEN);
            break;

        case 10: // SYSTEM STATUS -> Engrane
            tft.drawCircle(cx, cy, 15, MATRIX_GREEN);
            tft.drawCircle(cx, cy, 5, MATRIX_GREEN);
            for (int i = 0; i < 8; i++) {
                float r = i * 0.785;
                tft.fillCircle(cx + cos(r) * 23, cy + sin(r) * 23, 3, MATRIX_GREEN);
            }
            break;

        

        case 11: // GAMES ARCADE -> Gamepad Retro
            tft.drawRoundRect(cx - 25, cy - 12, 50, 24, 8, MATRIX_GREEN); // Cuerpo del control
            // Cruceta (D-Pad)
            tft.drawFastVLine(cx - 15, cy - 7, 14, MATRIX_GREEN);
            tft.drawFastHLine(cx - 22, cy, 14, MATRIX_GREEN);
            // Botones de acción
            tft.fillCircle(cx + 12, cy - 5, 2, MATRIX_GREEN);
            tft.fillCircle(cx + 18, cy, 2, MATRIX_GREEN);
            tft.fillCircle(cx + 12, cy + 5, 2, MATRIX_GREEN);
            // Start/Select
            tft.drawFastHLine(cx - 4, cy + 4, 3, MATRIX_GREEN);
            tft.drawFastHLine(cx + 1, cy + 4, 3, MATRIX_GREEN);
            break;

        case 12: // BLE JAMMER -> Bluetooth con Interferencia
            // Símbolo de Bluetooth central
            tft.drawLine(cx, cy - 20, cx, cy + 20, MATRIX_GREEN);
            tft.drawLine(cx, cy - 20, cx + 10, cy - 10, MATRIX_GREEN);
            tft.drawLine(cx + 10, cy - 10, cx - 10, cy + 10, MATRIX_GREEN);
            tft.drawLine(cx - 10, cy - 10, cx + 10, cy + 10, MATRIX_GREEN);
            tft.drawLine(cx + 10, cy + 10, cx, cy + 20, MATRIX_GREEN);
            
            // Ondas de interferencia (Rayos laterales)
            tft.drawLine(cx - 25, cy - 10, cx - 15, cy, MATRIX_GREEN); // Rayo Izq
            tft.drawLine(cx - 15, cy, cx - 25, cy + 10, MATRIX_GREEN);
            
            tft.drawLine(cx + 25, cy - 10, cx + 15, cy, MATRIX_GREEN); // Rayo Der
            tft.drawLine(cx + 15, cy, cx + 25, cy + 10, MATRIX_GREEN);
            break;

        case 13: // ABOUT SYSTEM -> Icono de Información (i)
            tft.drawCircle(cx, cy, 22, MATRIX_GREEN); // Círculo interno
            tft.fillCircle(cx, cy - 12, 3, MATRIX_GREEN); // Punto de la 'i'
            tft.fillRect(cx - 2, cy - 5, 5, 18, MATRIX_GREEN); // Cuerpo de la 'i'
            tft.drawFastHLine(cx - 6, cy + 13, 13, MATRIX_GREEN); // Base de la 'i'
            break;


    }
    tft.endWrite();
}