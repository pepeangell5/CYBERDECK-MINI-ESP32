#include "games_menu.h"
#include "display_config.h" // Aquí ya viene el extern LGFX_Sprite* globalCanvas;
#include "games/doom_port.h"
#include "games/arkanoid.h"
#include "games/dino_game.h"
#include "games/tetris.h"
#include "games/space_shooter.h"
#include "games/snake.h"
#include "games/cyber_runner.h" // Nuevo include para el juego 3D

// --- NO AGREGUES DECLARACIONES DE globalCanvas AQUÍ ---

void showGamesMenu(LGFX &tft, bool &inSubMenu, int btnBack) {
    int gameIndex = 0;
    int scrollOffset = 0;
    const int visibleItems = 5;
    const int totalGames = 7; // Actualizado a 7 juegos
    const char* gameList[] = { 
        "1. MATRIX SNAKE", 
        "2. CYBER DOOM", 
        "3. NEON ARKANOID",
        "4. CHROME DINO",
        "5. CYBER TETRIS",
        "6. CYBER ATTACK",
        "7. CYBER RUNNER"  // Añadido Cyber Runner
    };

    bool exitMenu = false;
    bool updateDisplay = true; 

    // Pines de control PepeAngell
    const int B_UP = 1, B_DOWN = 2, B_ENTER = 42, B_BACK = 41, E_SW = 3;
    const int E_CLK = 48, E_DT = 46;
    int lastClk = digitalRead(E_CLK);

    tft.fillScreen(TFT_BLACK);

    while (!exitMenu) {
        if (updateDisplay) {
            tft.startWrite();
            tft.fillScreen(TFT_BLACK);
            
            // Marco estético Cyberdeck
            tft.drawRect(10, 10, 300, 220, 0x07E0); // MATRIX_GREEN
            tft.fillRect(10, 10, 300, 35, 0x07E0);
            
            tft.setTextColor(TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(85, 18);
            tft.print("ARCADE ZONE");

            // Lógica de Scroll (Ventana de 5 elementos)
            if (gameIndex < scrollOffset) {
                scrollOffset = gameIndex;
            } else if (gameIndex >= scrollOffset + visibleItems) {
                scrollOffset = gameIndex - visibleItems + 1;
            }

            for(int i = 0; i < visibleItems; i++) {
                int actualIndex = i + scrollOffset;
                if (actualIndex >= totalGames) break;
                
                int yPos = 55 + (i * 33);
                if(actualIndex == gameIndex) {
                    tft.fillRect(20, yPos - 4, 280, 29, 0x03E0); 
                    tft.setTextColor(TFT_WHITE);
                } else {
                    tft.setTextColor(0x07E0);
                }
                tft.setCursor(30, yPos);
                tft.print(gameList[actualIndex]);
            }

            // Triángulos de Scroll
            if (scrollOffset > 0) tft.fillTriangle(295, 50, 305, 50, 300, 43, 0x07E0);
            if (scrollOffset + visibleItems < totalGames) tft.fillTriangle(295, 215, 305, 215, 300, 222, 0x07E0);

            tft.setTextColor(0x07E0);
            tft.setTextSize(1);
            tft.setCursor(85, 220);
            tft.print("HOLD ENCODER TO EXIT");
            tft.endWrite();
            updateDisplay = false; 
        }

        // Navegación
        bool moved = false;
        if (digitalRead(B_UP) == LOW) { gameIndex = (gameIndex <= 0) ? totalGames - 1 : gameIndex - 1; moved = true; while(digitalRead(B_UP) == LOW) yield(); }
        if (digitalRead(B_DOWN) == LOW) { gameIndex = (gameIndex >= totalGames - 1) ? 0 : gameIndex + 1; moved = true; while(digitalRead(B_DOWN) == LOW) yield(); }

        int currentClk = digitalRead(E_CLK);
        if (currentClk != lastClk && currentClk == LOW) {
            if (digitalRead(E_DT) != currentClk) gameIndex = (gameIndex >= totalGames - 1) ? 0 : gameIndex + 1;
            else gameIndex = (gameIndex <= 0) ? totalGames - 1 : gameIndex - 1;
            moved = true; delay(5);
        }
        lastClk = currentClk;
        
        if (moved) { updateDisplay = true; tone(14, 1200, 15); }

        // Selección
        if (digitalRead(B_ENTER) == LOW || digitalRead(E_SW) == LOW) {
            unsigned long pressStart = millis();
            bool isLong = false;
            while(digitalRead(B_ENTER) == LOW || digitalRead(E_SW) == LOW) {
                if (millis() - pressStart > 600) { isLong = true; break; }
                yield();
            }

            if (isLong) {
                exitMenu = true;
                inSubMenu = false;
            } else {
                bool playingVar = true;
                if (gameIndex == 0) launchSnake(tft, playingVar, B_BACK);
                else if (gameIndex == 1) launchDoom(tft, playingVar, B_BACK);
                else if (gameIndex == 2) launchArkanoid(tft, playingVar, B_BACK);
                else if (gameIndex == 3) launchDino(tft);
                else if (gameIndex == 4) launchTetris(tft);
                else if (gameIndex == 5) launchSpaceShooter(tft);
                else if (gameIndex == 6) launchCyberRunner(tft); // Lanzar el nuevo juego 3D

                delay(300);
                while(digitalRead(B_BACK) == LOW || digitalRead(B_ENTER) == LOW || digitalRead(E_SW) == LOW) yield();
                tft.fillScreen(TFT_BLACK); 
                updateDisplay = true;      
            }
            while(digitalRead(B_ENTER) == LOW || digitalRead(E_SW) == LOW) yield();
        }
        
        if (digitalRead(B_BACK) == LOW) { exitMenu = true; inSubMenu = false; while(digitalRead(B_BACK) == LOW) yield(); }
        yield();
    }
}