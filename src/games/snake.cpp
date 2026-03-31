#include "games/snake.h"
#include <vector>

#define MATRIX_GREEN 0x07E0
#define MATRIX_DARK  0x01E0

struct Point { int x, y; };

void launchSnake(LGFX &tft, bool &inSubMenu, int btnBack) {
    const int gridSize = 10;
    const int width = 320 / gridSize;
    const int height = 240 / gridSize;
    
    std::vector<Point> snake = {{16, 12}, {15, 12}, {14, 12}};
    Point food = {rand() % (width-2) + 1, rand() % (height-4) + 3}; 
    
    int dir = 1; // 0:UP, 1:RIGHT, 2:DOWN, 3:LEFT
    int score = 0;
    int lastScore = -1; 
    unsigned long lastMove = 0;
    int speed = 150;

    // --- PINES ---
    const int B_UP = 1, B_DOWN = 2, B_BACK = 41, B_ENTER = 42, E_SW = 3;
    const int E_CLK = 48, E_DT = 46;

    int lastClk = digitalRead(E_CLK);

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 20, 320, 220, MATRIX_GREEN); 

    while (inSubMenu) {
        // --- 1. DIBUJAR PUNTAJE ---
        if (score != lastScore) {
            tft.startWrite();
            tft.fillRect(0, 0, 320, 20, MATRIX_GREEN);
            tft.setTextColor(TFT_BLACK);
            tft.setTextSize(1);
            tft.setCursor(10, 6);
            tft.printf(" SCORE: %04d   |   SPEED: %dms   |   CYBERDECK ARCADE", score, speed);
            tft.endWrite();
            lastScore = score;
        }

        // --- 2. CONTROL DE SALIDA (BACK O CLICK ENCODER) ---
        if (digitalRead(B_BACK) == LOW || digitalRead(E_SW) == LOW) {
            inSubMenu = false; 
            while(digitalRead(B_BACK) == LOW || digitalRead(E_SW) == LOW) yield(); 
            return; 
        }

        // --- 3. CONTROL DE GIRO CON DEBOUNCE REFORZADO ---
        int currentClk = digitalRead(E_CLK);
        
        // Giro Anti-horario (Botón 1)
        if (digitalRead(B_UP) == LOW) {
            dir = (dir + 3) % 4; 
            delay(180); // Aumentado a 180ms para filtrar ruidos mecánicos
            while(digitalRead(B_UP) == LOW) yield(); // BLOQUEO hasta soltar el botón
            delay(50); // Pequeño margen extra al soltar
        }
        // Giro Horario (Botón 2)
        else if (digitalRead(B_DOWN) == LOW) {
            dir = (dir + 1) % 4; 
            delay(180); // Aumentado a 180ms
            while(digitalRead(B_DOWN) == LOW) yield(); // BLOQUEO hasta soltar el botón
            delay(50); // Pequeño margen extra al soltar
        }
        // Control por Encoder (Sigue igual de fluido)
        else if (currentClk != lastClk && currentClk == LOW) {
            if (digitalRead(E_DT) != currentClk) dir = (dir + 1) % 4; 
            else dir = (dir + 3) % 4; 
        }
        lastClk = currentClk;

        // --- 4. LÓGICA DE MOVIMIENTO ---
        if (millis() - lastMove > speed) {
            Point next = snake[0];
            if (dir == 0)      next.y--;
            else if (dir == 1) next.x++;
            else if (dir == 2) next.y++;
            else if (dir == 3) next.x--;

            // Colisiones bordes
            if (next.x < 0 || next.x >= width || (next.y * gridSize) < 20 || next.y >= height) break;
            
            // Colisión cuerpo
            bool selfHit = false;
            for (auto &p : snake) {
                if (p.x == next.x && p.y == next.y) { selfHit = true; break; }
            }
            if (selfHit) break;

            snake.insert(snake.begin(), next);

            if (next.x == food.x && next.y == food.y) {
                score += 10;
                speed = (speed > 50) ? speed - 2 : 50; 
                food = {rand() % (width-2) + 1, rand() % (height-4) + 3}; 
                digitalWrite(14, HIGH); delay(5); digitalWrite(14, LOW); 
            } else {
                Point last = snake.back();
                tft.fillRect(last.x * gridSize + 1, last.y * gridSize + 1, gridSize - 2, gridSize - 2, TFT_BLACK);
                snake.pop_back();
            }

            // Dibujar
            tft.fillRect(food.x * gridSize + 1, food.y * gridSize + 1, gridSize - 2, gridSize - 2, TFT_RED);
            tft.fillRect(snake[0].x * gridSize + 1, snake[0].y * gridSize + 1, gridSize - 2, gridSize - 2, MATRIX_GREEN);
            if(snake.size() > 1) {
                tft.fillRect(snake[1].x * gridSize + 1, snake[1].y * gridSize + 1, gridSize - 2, gridSize - 2, MATRIX_DARK);
            }
            lastMove = millis();
        }
        yield();
    }

    // PANTALLA DE GAME OVER
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED); tft.setTextSize(3);
    tft.setCursor(65, 90); tft.print("TERMINATED_");
    tft.setTextColor(MATRIX_GREEN); tft.setTextSize(2);
    tft.setCursor(110, 140); tft.printf("SCORE: %d", score);
    digitalWrite(14, HIGH); delay(150); digitalWrite(14, LOW);
    delay(2000);
    
    inSubMenu = false;
}