#include "games_menu.h"
#include "games/doom_port.h"
#include "games/arkanoid.h"

void showGamesMenu(LGFX &tft, bool &inSubMenu, int btnBack) {
    int gameIndex = 0;
    const int totalGames = 3; // Actualizado a 3 juegos
    const char* gameList[] = { 
        "1. MATRIX SNAKE", 
        "2. CYBER DOOM", 
        "3. NEON ARKANOID" 
    };
    bool exitMenu = false;
    bool updateDisplay = true; 

    // Pines de control PepeAngell
    const int B_UP = 1, B_DOWN = 2, B_ENTER = 42, B_BACK = 41, E_SW = 3;
    const int E_CLK = 48, E_DT = 46;
    
    int lastClk = digitalRead(E_CLK);

    while (!exitMenu) {
        if (updateDisplay) {
            tft.startWrite();
            tft.fillScreen(TFT_BLACK);
            
            // Marco estético
            tft.drawRect(10, 10, 300, 220, MATRIX_GREEN);
            tft.fillRect(10, 10, 300, 30, MATRIX_GREEN);
            
            tft.setTextColor(TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(85, 17);
            tft.print("ARCADE ZONE");

            // Lista de juegos
            for(int i = 0; i < totalGames; i++) {
                if(i == gameIndex) {
                    tft.fillRect(20, 60 + (i * 35), 280, 30, 0x03E0); 
                    tft.setTextColor(TFT_WHITE);
                } else {
                    tft.setTextColor(MATRIX_GREEN);
                }
                tft.setCursor(30, 68 + (i * 35));
                tft.print(gameList[i]);
            }

            tft.setTextColor(MATRIX_GREEN);
            tft.setTextSize(1);
            tft.setCursor(85, 215);
            tft.print("HOLD ENCODER TO EXIT");
            tft.endWrite();
            updateDisplay = false; 
        }

        // --- 1. NAVEGACIÓN POR BOTONES ---
        if (digitalRead(B_UP) == LOW) {
            gameIndex = (gameIndex <= 0) ? totalGames - 1 : gameIndex - 1;
            updateDisplay = true;
            while(digitalRead(B_UP) == LOW) yield();
        }
        
        if (digitalRead(B_DOWN) == LOW) {
            gameIndex = (gameIndex >= totalGames - 1) ? 0 : gameIndex + 1;
            updateDisplay = true;
            while(digitalRead(B_DOWN) == LOW) yield();
        }

        // --- 2. NAVEGACIÓN POR ENCODER ---
        int currentClk = digitalRead(E_CLK);
        if (currentClk != lastClk && currentClk == LOW) {
            if (digitalRead(E_DT) != currentClk) {
                gameIndex = (gameIndex >= totalGames - 1) ? 0 : gameIndex + 1;
            } else {
                gameIndex = (gameIndex <= 0) ? totalGames - 1 : gameIndex - 1;
            }
            updateDisplay = true;
            delay(5); 
        }
        lastClk = currentClk;

        // --- 3. SELECCIÓN Y SALIDA ---
        if (digitalRead(B_ENTER) == LOW || digitalRead(E_SW) == LOW) {
            unsigned long pressStart = millis();
            bool isLongPress = false;
            while(digitalRead(B_ENTER) == LOW || digitalRead(E_SW) == LOW) {
                if (millis() - pressStart > 600) { isLongPress = true; break; }
                yield();
            }

            if (isLongPress) {
                exitMenu = true;
                inSubMenu = false;
            } else {
                bool playing = true;
                if (gameIndex == 0) {
                    launchSnake(tft, playing, B_BACK);
                } 
                else if (gameIndex == 1) {
                    launchDoom(tft, playing, B_BACK);
                }
                else if (gameIndex == 2) { // NUEVO JUEGO
                    launchArkanoid(tft, playing, B_BACK);
                }
                updateDisplay = true; 
            }
            while(digitalRead(B_ENTER) == LOW || digitalRead(E_SW) == LOW) yield();
        }
        
        // --- 4. BOTÓN ATRÁS FÍSICO ---
        if (digitalRead(B_BACK) == LOW) {
            exitMenu = true;
            inSubMenu = false;
            while(digitalRead(B_BACK) == LOW) yield();
        }
        
        yield();
    }
}