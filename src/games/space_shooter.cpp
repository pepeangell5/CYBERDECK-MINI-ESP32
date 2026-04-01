#include "games/space_shooter.h"

// Pines Cyberdeck PepeAngell
const int S_LEFT   = 1;  
const int S_RIGHT  = 2;  
const int S_FIRE   = 42; 
const int S_BACK   = 41; 
const int S_BUZZER = 14;

// Estructuras de juego
struct Bullet { float x, y; bool active; };
struct Enemy { float x, y; bool active; int type; };

// Variables locales estáticas (No globales para no romper otros archivos)
static const int MAX_BULLETS = 5;
static const int MAX_ENEMIES = 8;
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];

void spawnEnemy(int i) {
    enemies[i].x = random(20, 280);
    enemies[i].y = random(-100, -20);
    enemies[i].active = true;
    enemies[i].type = random(0, 2);
}

void launchSpaceShooter(LGFX &tft) {
    // 1. Limpieza de entrada
    delay(200);
    while(digitalRead(S_FIRE) == LOW || digitalRead(S_BACK) == LOW) { yield(); }

    // 2. Crear Sprite LOCAL (Se crea y se destruye aquí mismo para no estorbar)
    LGFX_Sprite s_canvas(&tft); 
    s_canvas.setColorDepth(8);
    if (!s_canvas.createSprite(320, 240)) {
        return; // Si no hay RAM, abortamos antes de que truene
    }

    // 3. Reset de variables de juego
    float shipX = 160;
    int s_score = 0;
    bool playing = true;

    for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = false;
    for(int i=0; i<MAX_ENEMIES; i++) spawnEnemy(i);

    tft.fillScreen(TFT_BLACK); 

    while (playing) {
        unsigned long loopStart = millis();

        // CONTROLES
        if (digitalRead(S_LEFT) == LOW && shipX > 10) shipX -= 5.0;
        if (digitalRead(S_RIGHT) == LOW && shipX < 300) shipX += 5.0;
        
        if (digitalRead(S_FIRE) == LOW) {
            for(int i=0; i<MAX_BULLETS; i++) {
                if (!bullets[i].active) {
                    bullets[i].x = shipX + 8;
                    bullets[i].y = 205;
                    bullets[i].active = true;
                    tone(S_BUZZER, 1600, 10);
                    break;
                }
            }
            delay(120); 
        }

        // FÍSICA Y COLISIONES
        for(int i=0; i<MAX_BULLETS; i++) {
            if (bullets[i].active) {
                bullets[i].y -= 7;
                if (bullets[i].y < 0) bullets[i].active = false;
            }
        }

        for(int i=0; i<MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                enemies[i].y += 1.8 + (s_score / 15.0);
                if (enemies[i].y > 240) spawnEnemy(i);

                for(int j=0; j<MAX_BULLETS; j++) {
                    if (bullets[j].active && abs(bullets[j].x - enemies[i].x) < 18 && abs(bullets[j].y - enemies[i].y) < 18) {
                        enemies[i].active = false; bullets[j].active = false;
                        s_score++; tone(S_BUZZER, 300, 15); spawnEnemy(i);
                    }
                }
                if (abs(shipX + 10 - (enemies[i].x + 7)) < 15 && enemies[i].y > 200) playing = false;
            }
        }

        // RENDER
        s_canvas.fillSprite(TFT_BLACK);
        for(int i=0; i<15; i++) s_canvas.drawPixel(random(320), random(240), 0x7BEF);
        
        s_canvas.fillRect(shipX + 8, 210, 4, 10, 0x07E0); 
        s_canvas.fillRect(shipX, 220, 20, 8, 0x03E0);    

        for(int i=0; i<MAX_BULLETS; i++) if (bullets[i].active) s_canvas.fillRect(bullets[i].x, bullets[i].y, 3, 7, TFT_YELLOW);
        for(int i=0; i<MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                uint16_t eCol = (enemies[i].type == 0) ? TFT_RED : TFT_MAGENTA;
                s_canvas.fillTriangle(enemies[i].x, enemies[i].y, enemies[i].x+15, enemies[i].y, enemies[i].x+7, enemies[i].y+12, eCol);
            }
        }

        s_canvas.setTextColor(TFT_WHITE); s_canvas.setTextSize(2);
        s_canvas.setCursor(10, 10); s_canvas.printf("SCORE: %03d", s_score);
        
        s_canvas.pushSprite(0, 0);

        if (digitalRead(S_BACK) == LOW) { playing = false; delay(200); }
        yield();
    }

    // GAME OVER
    s_canvas.fillSprite(TFT_BLACK);
    s_canvas.setTextColor(TFT_RED); s_canvas.setTextSize(3);
    s_canvas.setCursor(60, 100); s_canvas.print("SYSTEM HALT");
    s_canvas.pushSprite(0, 0);
    tone(S_BUZZER, 100, 600);
    delay(1500);

    // 4. Liberar memoria al salir (Automático al ser objeto local, pero forzamos)
    s_canvas.deleteSprite();
    tft.fillScreen(TFT_BLACK);
}