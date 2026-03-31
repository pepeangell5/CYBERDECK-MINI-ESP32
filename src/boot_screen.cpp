#include "boot_screen.h"

void drawBootScreen(LGFX &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(MATRIX_GREEN);
    
    // Dibujo de marco inicial
    for(int i=0; i<10; i++) {
        tft.drawRect(i, i, tft.width()-(i*2), tft.height()-(i*2), MATRIX_GREEN);
        delay(20);
    }
    
    // Banner superior
    tft.fillRect(0, 0, tft.width(), 35, MATRIX_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(15, 12);
    tft.print("INICIALIZANDO_NUCLEO...");
    
    tft.setTextColor(MATRIX_GREEN);
    
    // Letras de CYBERDECK (Efecto Glitch rápido)
    String cybertext = "CYBERDECK";
    int startX = 35;
    int startY = 85;
    
    tft.setTextSize(3);
    for (int i = 0; i < cybertext.length(); i++) {
        tft.setCursor(startX + (i * 22), startY);
        tft.print(cybertext[i]);
        delay(60);
        // Pequeño destello
        tft.drawFastVLine(startX + (i * 22) + 5, startY - 5, 30, TFT_WHITE);
        delay(20);
        tft.drawFastVLine(startX + (i * 22) + 5, startY - 5, 30, TFT_BLACK);
    }
    
    tft.drawFastHLine(30, startY + 35, tft.width() - 60, MATRIX_GREEN);
    
    // Texto PepeAngell (Typewriter)
    tft.setTextSize(1);
    tft.setCursor(startX + 10, startY + 50);
    String operatorText = "OPERADOR_AUTORIZADO: PepeAngell";
    
    for(int i=0; i < operatorText.length(); i++) {
        tft.print(operatorText[i]);
        delay(50);
    }
    
    // Barra de carga final
    int barW = tft.width() - 80;
    tft.drawRect(40, 190, barW, 15, MATRIX_GREEN);
    for(int i=0; i < barW - 4; i++) {
        tft.fillRect(42 + i, 192, 1, 11, MATRIX_GREEN);
        if(i % 5 == 0) delay(10);
    }
    
    tft.setCursor(90, 215);
    tft.print("[ ACCESO CONCEDIDO ]");
    delay(1000);
}