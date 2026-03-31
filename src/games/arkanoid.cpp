#include "games/arkanoid.h"
#include <vector>
#include <Preferences.h>

struct Ball { float x, y, dx, dy; };
struct PowerUp { float x, y; char type; bool active; };
struct Brick { bool active; uint16_t color; int health; };
struct Bullet { float x, y; bool active; };

const uint8_t levelShapes[4][5][8] = {
    {{0,1,1,0,0,1,1,0}, {1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1}, {0,1,1,1,1,1,1,0}, {0,0,1,1,1,1,0,0}}, // LVL 6
    {{0,0,1,1,1,1,0,0}, {0,1,1,1,1,1,1,0}, {0,1,0,1,1,0,1,0}, {0,1,1,1,1,1,1,0}, {0,0,1,0,0,1,0,0}}, // LVL 7
    {{0,0,1,0,0,1,0,0}, {0,1,1,1,1,1,1,0}, {1,1,0,1,1,0,1,1}, {1,1,1,1,1,1,1,1}, {0,1,0,1,1,0,1,0}}, // LVL 8
    {{0,0,0,1,1,0,0,0}, {0,0,0,1,1,0,0,0}, {1,1,1,1,1,1,1,1}, {0,0,0,1,1,0,0,0}, {0,0,0,1,1,0,0,0}}  // LVL 9
};

void launchArkanoid(LGFX &tft, bool &playing, int btnBack) {
    const int screenW = 320, screenH = 240, B_PIN = 14, rows = 5, cols = 8;
    pinMode(B_PIN, OUTPUT);
    auto beep = [&](int freq, int dur) { tone(B_PIN, freq, dur); };
    
    auto playGameOver = [&]() {
        int notes[] = {200, 150, 100, 80};
        for(int n : notes) { beep(n, 300); delay(350); }
    };

    Preferences prefs;
    prefs.begin("arkanoid", false);
    int maxLevelUnlocked = prefs.getInt("maxLevel", 1);
    
    int selectedLevel = 1;
    bool inLevelSelector = true, inGameActive = false, powerActive = false;
    int lives = 3, score = 0, currentLevel = 1, paddleW = 60;
    float paddleX = (screenW - 60) / 2, levelBaseSpeed = 3.0;
    unsigned long powerTimer = 0, bossShootTimer = 0;

    Brick bricks[rows][cols];
    uint16_t colors[] = {TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_BLUE, TFT_MAGENTA, TFT_CYAN};
    std::vector<Ball> balls;
    std::vector<PowerUp> powers;
    std::vector<Bullet> bossBullets;
    float bossX = 110, bossHealth = 150, bossDir = 1.8;

    auto resetLevel = [&](int level, bool isRetry) {
        currentLevel = level;
        if(!isRetry) { score = (level == 1) ? 0 : score; lives = 3; }
        levelBaseSpeed = 3.0 + (level * 0.45); 
        bossBullets.clear(); powers.clear(); balls.clear();
        balls.push_back({(float)screenW/2, (float)screenH-60, 0, -levelBaseSpeed});
        paddleW = 60; paddleX = (screenW - 60) / 2; powerActive = false;
        
        if(level <= 5) {
            for(int r = 0; r < rows; r++) for(int c = 0; c < cols; c++) {
                bricks[r][c].active = true;
                bricks[r][c].color = colors[rand()%7];
                if(level == 1) bricks[r][c].health = 1;
                else bricks[r][c].health = (rand() % 10 < 3) ? level : 1; 
            }
        } else if(level <= 9) {
            for(int r = 0; r < rows; r++) for(int c = 0; c < cols; c++) {
                bricks[r][c].active = (levelShapes[level-6][r][c] == 1);
                bricks[r][c].color = colors[level-6];
                if(bricks[r][c].active) bricks[r][c].health = (rand() % 5) + 1; // SALUD ALEATORIA EN FIGURAS
                else bricks[r][c].health = 0;
            }
        } else { 
            bossHealth = 150; // MÁS VIDA AL BOSS
            for(int r=0; r<rows; r++) for(int c=0; c<cols; c++) bricks[r][c].active = false; 
        }
    };

    const int B_UP = 1, B_DOWN = 2, B_BACK = 41, B_ENTER = 42, E_SW = 3, E_CLK = 48, E_DT = 46;
    int lastClk = digitalRead(E_CLK);
    LGFX_Sprite canvas(&tft); canvas.createSprite(screenW, screenH);

    while (playing) {
        while(inLevelSelector && playing) {
            canvas.fillScreen(TFT_BLACK); canvas.setTextSize(2);
            canvas.setTextColor(TFT_WHITE); canvas.setCursor(60, 10); canvas.print("SELECT MISSION");
            for(int i = 1; i <= 10; i++) {
                int drawY = 40 + (i - 1) * 18;
                bool locked = (i > maxLevelUnlocked + 1);
                canvas.setTextColor(i == selectedLevel ? TFT_YELLOW : (locked ? 0x4208 : TFT_WHITE));
                canvas.setTextSize(1); canvas.setCursor(85, drawY);
                canvas.printf("LEVEL %02d %s", i, (i == 10 ? "- MEGA BOSS" : (locked ? "[LOCKED]" : "OK")));
                if(i == selectedLevel) canvas.print(" <");
            }
            if(digitalRead(B_UP) == LOW) { selectedLevel = (selectedLevel <= 1) ? 10 : selectedLevel - 1; beep(400, 20); while(digitalRead(B_UP) == LOW) yield(); }
            if(digitalRead(B_DOWN) == LOW) { selectedLevel = (selectedLevel >= 10) ? 1 : selectedLevel + 1; beep(300, 20); while(digitalRead(B_DOWN) == LOW) yield(); }
            int clk = digitalRead(E_CLK);
            if (clk != lastClk && clk == LOW) {
                if (digitalRead(E_DT) != clk) selectedLevel = (selectedLevel >= 10) ? 1 : selectedLevel + 1;
                else selectedLevel = (selectedLevel <= 1) ? 10 : selectedLevel - 1;
            } lastClk = clk;
            if (digitalRead(E_SW) == LOW || digitalRead(B_ENTER) == LOW) {
                if (selectedLevel <= maxLevelUnlocked + 1) { resetLevel(selectedLevel, false); inLevelSelector = false; inGameActive = true; beep(600, 50); }
                while(digitalRead(B_ENTER) == LOW) yield();
            }
            if (digitalRead(B_BACK) == LOW) { playing = false; while(digitalRead(B_BACK) == LOW) yield(); }
            canvas.pushSprite(0,0); yield();
        }

        while (inGameActive && playing) {
            if (digitalRead(B_BACK) == LOW) { inGameActive = false; inLevelSelector = true; while(digitalRead(B_BACK) == LOW) yield(); break; }
            canvas.fillScreen(TFT_BLACK);
            bool anyB = false;
            if (powerActive && millis() > powerTimer) { paddleW = 60; powerActive = false; }
            float pSpd = 7.5 + (levelBaseSpeed * 0.3);
            if (digitalRead(B_UP) == LOW && paddleX > 0) paddleX -= pSpd;
            if (digitalRead(B_DOWN) == LOW && paddleX < screenW - paddleW) paddleX += pSpd;

            if(currentLevel == 10 && bossHealth > 0) {
                anyB = true; bossX += bossDir; if(bossX < 10 || bossX > 210) bossDir *= -1;
                if(millis() > bossShootTimer) {
                    bossBullets.push_back({bossX + 50, 60, true});
                    bossShootTimer = millis() + (bossHealth > 75 ? 1300 : 800); beep(200, 30);
                }
                canvas.fillRect(bossX, 35, 100, 25, TFT_RED);
                canvas.drawRect(bossX, 35, 100, 25, TFT_WHITE);
                canvas.setTextColor(TFT_WHITE); canvas.setTextSize(1); canvas.setCursor(bossX+40, 42); canvas.print((int)bossHealth);
            }

            for(int i=0; i<bossBullets.size(); i++) {
                bossBullets[i].y += 3.8; canvas.fillTriangle(bossBullets[i].x, bossBullets[i].y, bossBullets[i].x-6, bossBullets[i].y-12, bossBullets[i].x+6, bossBullets[i].y-12, TFT_RED);
                if(bossBullets[i].y > screenH) { bossBullets.erase(bossBullets.begin()+i); i--; continue; }
                if(bossBullets[i].y >= screenH-30 && bossBullets[i].x >= paddleX && bossBullets[i].x <= paddleX+paddleW) {
                    lives--; bossBullets.erase(bossBullets.begin()+i); i--; beep(80, 400); delay(600); break;
                }
            }

            for(int i=0; i < powers.size(); i++) {
                powers[i].y += 2.2;
                canvas.fillCircle(powers[i].x, powers[i].y, 6, (powers[i].type == 'C' ? TFT_MAGENTA : TFT_BLUE));
                canvas.setTextColor(TFT_WHITE); canvas.setTextSize(1); canvas.setCursor(powers[i].x-3, powers[i].y-4); canvas.print(powers[i].type);
                if(powers[i].y > screenH) { powers.erase(powers.begin()+i); i--; continue; }
                if(powers[i].y >= screenH-30 && powers[i].y <= screenH-15 && powers[i].x >= paddleX && powers[i].x <= paddleX+paddleW) {
                    powerActive = true; powerTimer = millis()+8000; beep(1300, 60);
                    if(powers[i].type=='C') paddleW = 100; else balls.push_back({paddleX+paddleW/2, (float)screenH-40, 1.5, -levelBaseSpeed});
                    powers.erase(powers.begin()+i); i--;
                }
            }

            for (int i = 0; i < balls.size(); i++) {
                balls[i].x += balls[i].dx; balls[i].y += balls[i].dy;
                if (balls[i].x <= 0 || balls[i].x >= screenW - 6) { balls[i].dx *= -1; beep(900, 5); }
                if (balls[i].y <= 0) { balls[i].dy *= -1; beep(900, 5); }
                if (balls[i].y >= screenH - 35 && balls[i].y <= screenH - 25) {
                    if (balls[i].x + 6 >= paddleX && balls[i].x <= paddleX + paddleW) {
                        float hP = ((balls[i].x + 3) - (paddleX + paddleW/2)) / (paddleW/2);
                        balls[i].dx = hP * levelBaseSpeed * 1.3; balls[i].dy = -sqrt(sq(levelBaseSpeed) - sq(balls[i].dx * 0.5));
                        balls[i].y = screenH - 36; beep(1100, 10);
                    }
                }
                if(currentLevel < 10) {
                    for(int r=0; r<rows; r++) for(int c=0; c<cols; c++) if(bricks[r][c].active) {
                        anyB = true; int bX = 20+c*37, bY = 40+r*17;
                        if (balls[i].x+6 > bX && balls[i].x < bX+35 && balls[i].y+6 > bY && balls[i].y < bY+15) {
                            float dL=(balls[i].x+6)-bX, dR=(bX+35)-balls[i].x, dT=(balls[i].y+6)-bY, dB=(bY+15)-balls[i].y;
                            float mD=fmin(fmin(dL,dR),fmin(dT,dB));
                            if (mD==dL || mD==dR) balls[i].dx*=-1; else balls[i].dy*=-1;
                            if (--bricks[r][c].health <= 0) { bricks[r][c].active=false; score+=25; beep(550, 15); if(rand()%100 < 18) powers.push_back({(float)bX+15, (float)bY, (rand()%2==0?'C':'M'), true}); }
                            else beep(450, 8); goto next_ball;
                        }
                    }
                } else if(bossHealth > 0 && balls[i].y < 60 && balls[i].x > bossX && balls[i].x < bossX+100) { balls[i].dy *= -1; bossHealth--; beep(450, 10); }
                next_ball:;
            }

            for (int i = 0; i < balls.size(); i++) if (balls[i].y > screenH) { balls.erase(balls.begin()+i); i--; }
            if (balls.empty()) {
                lives--; beep(150, 300);
                if (lives <= 0) { canvas.setTextColor(TFT_RED); canvas.setTextSize(3); canvas.setCursor(65, 100); canvas.print("GAME OVER"); canvas.pushSprite(0,0); playGameOver(); delay(2000); inGameActive=false; inLevelSelector=true; break; } 
                else { balls.push_back({(float)screenW/2, (float)screenH-60, 0, -levelBaseSpeed}); paddleX=(screenW-paddleW)/2; delay(800); }
            }

            if (!anyB && !balls.empty()) {
                if(currentLevel < 10) { if(currentLevel >= maxLevelUnlocked) { maxLevelUnlocked=currentLevel; prefs.putInt("maxLevel", maxLevelUnlocked); } resetLevel(currentLevel+1, false); }
                else { canvas.setTextColor(TFT_GREEN); canvas.setTextSize(3); canvas.setCursor(60, 100); canvas.print("VICTORY!"); canvas.pushSprite(0,0); beep(800, 200); beep(1200, 500); delay(3000); inGameActive=false; inLevelSelector=true; break; }
            }

            if(currentLevel < 10) {
                for(int r=0; r<rows; r++) for(int c=0; c<cols; c++) if(bricks[r][c].active) {
                    canvas.fillRect(20+c*37, 40+r*17, 35, 15, bricks[r][c].color);
                    if(bricks[r][c].health > 1) { canvas.setTextColor(TFT_BLACK); canvas.setTextSize(1); canvas.setCursor(20+c*37+14, 40+r*17+4); canvas.print(bricks[r][c].health); }
                }
            }
            canvas.fillRect(paddleX, screenH-30, paddleW, 10, powerActive?0xF81F:0x07FF);
            for(auto& b : balls) canvas.fillCircle(b.x, b.y, 3, TFT_WHITE);
            canvas.setTextColor(TFT_WHITE); canvas.setTextSize(1); canvas.setCursor(10, 10); canvas.printf("SCORE: %05d  LVL: %d  LIVES: %d", score, currentLevel, lives);
            canvas.pushSprite(0,0); yield();
        }
    }
    prefs.end(); canvas.deleteSprite();
}