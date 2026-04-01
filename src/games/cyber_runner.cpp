#include "games/cyber_runner.h"
#include <math.h>

// Pines Cyberdeck PepeAngell
const int R_LEFT = 1, R_RIGHT = 2, R_BACK = 41, R_BUZZER = 14;

enum EnemyType { DRONE, MINE, PULSE };

struct Star { 
    float x, y, z; 
    EnemyType type;
    float angle;      
    float rotSpeed;
    bool active; 
};

const int MAX_STARS = 22; // Menos cantidad, más calidad visual
static Star stars[MAX_STARS];

void drawCyberShip(LGFX_Sprite *canvas, int x, int y, float angle, uint16_t color) {
    float scale = 0.9;
    auto rX = [&](float px, float py) { return px * cos(angle) - py * sin(angle) + x; };
    auto rY = [&](float px, float py) { return px * sin(angle) + py * cos(angle) + y; };

    canvas->fillTriangle(rX(-8*scale, 8*scale), rY(-8*scale, 8*scale), 
                         rX(8*scale, 8*scale), rY(8*scale, 8*scale), 
                         rX(0, 0), rY(0, 0), 0x03E0);
    canvas->fillTriangle(rX(0, -12*scale), rY(0, -12*scale), 
                         rX(-4*scale, 5*scale), rY(-4*scale, 5*scale), 
                         rX(4*scale, 5*scale), rY(4*scale, 5*scale), color);
    canvas->fillCircle(rX(0, -2*scale), rY(0, -2*scale), 2*scale, TFT_CYAN);
    canvas->fillCircle(rX(0, 7*scale), rY(0, 7*scale), 3*scale, TFT_RED);
}

void spawnEnemy(int i, bool firstTime = false) {
    stars[i].z = firstTime ? random(300, 900) : 900; 
    stars[i].angle = (random(0, 360) * PI) / 180.0;
    stars[i].active = true;
    
    int r = random(0, 100);
    if (r < 70) stars[i].type = DRONE; 
    else if (r < 90) {
        stars[i].type = MINE;  
        stars[i].rotSpeed = (random(0, 2) == 0 ? 0.03 : -0.03); 
    } else stars[i].type = PULSE;

    float radius = (stars[i].type == PULSE) ? 110 : 100;
    stars[i].x = cos(stars[i].angle) * radius;
    stars[i].y = sin(stars[i].angle) * radius;
}

void launchCyberRunner(LGFX &tft) {
    LGFX_Sprite canvas(&tft);
    canvas.setColorDepth(8);
    if (!canvas.createSprite(320, 240)) return;

    for(int i=0; i<MAX_STARS; i++) { stars[i].active = false; spawnEnemy(i, true); }

    float playerAngle = PI / 2;
    int score = 0;
    float speed = 3.5; // Empezamos un pelín más lento para testear
    bool playing = true;

    while (playing) {
        unsigned long startTime = millis();
        if (digitalRead(R_LEFT) == LOW) playerAngle -= 0.08;
        if (digitalRead(R_RIGHT) == LOW) playerAngle += 0.08;

        canvas.fillSprite(TFT_BLACK);

        for(int i=0; i<MAX_STARS; i++) {
            if (!stars[i].active) continue;
            stars[i].z -= speed;

            if (stars[i].type == MINE) {
                stars[i].angle += stars[i].rotSpeed;
                stars[i].x = cos(stars[i].angle) * 100;
                stars[i].y = sin(stars[i].angle) * 100;
            }

            if (stars[i].z <= 1) { stars[i].active = false; spawnEnemy(i); score++; continue; }

            // --- PROYECCIÓN AJUSTADA (Factor 220 para que crezcan más) ---
            float factor = 220.0 / stars[i].z; 
            int sx = (stars[i].x * factor) + 160;
            int sy = (stars[i].y * factor) + 120;
            
            if (stars[i].z > 5 && stars[i].z < 900) {
                if (stars[i].type == DRONE) {
                    canvas.drawCircle(sx, sy, 4*factor, 0x07E0);
                    canvas.drawCircle(sx, sy, 2*factor, TFT_WHITE);
                } 
                else if (stars[i].type == MINE) {
                    canvas.fillCircle(sx, sy, 4*factor, TFT_RED);
                    canvas.drawCircle(sx, sy, 5*factor, TFT_WHITE);
                } 
                else if (stars[i].type == PULSE) {
                    uint16_t pCol = (millis() % 300 < 150) ? TFT_CYAN : 0x001F; 
                    canvas.drawRect(sx - 6*factor, sy - 6*factor, 12*factor, 12*factor, pCol);
                }
            }

            // --- COLISIÓN AJUSTADA A LA VISTA ---
            // Solo colisiona cuando el enemigo está "encima" del jugador visualmente
            if (stars[i].active && stars[i].z < 25 && stars[i].z > 5) {
                float diff = fmod(stars[i].angle - playerAngle + PI, 2 * PI) - PI;
                float hitBox = (stars[i].type == PULSE) ? 0.30 : 0.15; 
                
                if (abs(diff) < hitBox) {
                    playing = false;
                    tone(R_BUZZER, 80, 400);
                }
            }
        }

        // Nave en Radio 90 (más cerca del borde del dibujo)
        int shipX = cos(playerAngle) * 90 + 160;
        int shipY = sin(playerAngle) * 90 + 120;
        drawCyberShip(&canvas, shipX, shipY, playerAngle + (PI/2), 0x07E0);

        canvas.setTextColor(0x07E0); canvas.setTextSize(2);
        canvas.setCursor(10, 10); canvas.printf("SCORE: %d", score);
        
        canvas.pushSprite(0, 0);
        if (digitalRead(R_BACK) == LOW) playing = false;
        
        speed += 0.0005; 
        while(millis() - startTime < 16) yield();
    }

    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_RED); canvas.setTextSize(3);
    canvas.drawCenterString("CORE FAILURE", 160, 100);
    canvas.pushSprite(0, 0);
    delay(1500);
    canvas.deleteSprite();
}