#include "games/dino_game.h"

// Pines Cyberdeck PepeAngell
const int D_UP_BTN   = 1;  
const int D_DOWN_BTN = 2;  
const int D_JUMP_BTN = 42; 
const int D_JUMP_ENC = 3;  
const int D_BUZZER   = 14; 
const int D_BACK     = 41; 

// Variables de estado
static float d_y, d_v, cactus_x, bird_x, speed;
static bool is_ducking, is_jumping, playing;
static int score, cactus_type, frame_count;

// Sprite puntero (Solución para el error de 'tft')
static LGFX_Sprite* canvas = nullptr;

void drawDetailedDino(LGFX_Sprite &spr, int x, int y, bool ducking, int frame, bool jumping) {
    if (ducking && !jumping) {
        spr.fillRect(x, y + 10, 32, 12, TFT_BLACK); 
        spr.fillRect(x + 22, y + 4, 14, 8, TFT_BLACK); 
        spr.fillRect(x + 30, y + 6, 2, 2, TFT_WHITE); 
    } else {
        spr.fillRect(x + 12, y - 12, 16, 12, TFT_BLACK); 
        spr.fillRect(x + 22, y - 10, 3, 3, TFT_WHITE);  
        spr.fillRect(x + 4, y, 14, 20, TFT_BLACK);      
        spr.fillRect(x, y + 4, 4, 10, TFT_BLACK);       
        spr.fillRect(x + 16, y + 8, 6, 4, TFT_BLACK);   
        
        int leg = (frame % 6 < 3) ? 0 : 8;
        if (jumping) leg = 4;
        spr.fillRect(x + 5, y + 20, 4, 6, TFT_BLACK);
        spr.fillRect(x + 13, y + 20, 4, 6, TFT_BLACK);
    }
}

void drawRealCactus(LGFX_Sprite &spr, int x, int y, int type) {
    switch (type) {
        case 0: spr.fillRect(x + 6, y, 6, 30, TFT_BLACK); spr.fillRect(x + 2, y + 8, 14, 4, TFT_BLACK); break;
        case 1: spr.fillRect(x, y + 8, 5, 22, TFT_BLACK); spr.fillRect(x + 8, y, 6, 30, TFT_BLACK); spr.fillRect(x, y + 15, 12, 4, TFT_BLACK); break;
        case 2: spr.fillRect(x + 10, y, 6, 30, TFT_BLACK); spr.fillRect(x, y + 12, 26, 4, TFT_BLACK); spr.fillRect(x, y + 6, 4, 7, TFT_BLACK); spr.fillRect(x + 22, y + 6, 4, 7, TFT_BLACK); break;
    }
}

void launchDino(LGFX &tft) {
    // 1. GESTIÓN DEL CANVAS (Crear una sola vez)
    if (canvas == nullptr) {
        canvas = new LGFX_Sprite(&tft);
        canvas->setColorDepth(8);
        if (!canvas->createSprite(320, 240)) {
            delete canvas;
            canvas = nullptr;
            return;
        }
    }

    // 2. RESET TOTAL (Garantiza reentrada limpia)
    score = 0; cactus_x = 320.0; bird_x = 800.0;
    d_y = 160.0; speed = 4.5; d_v = 0.0;
    is_jumping = false; is_ducking = false;
    playing = true; frame_count = 0;
    cactus_type = random(0, 3);

    tft.startWrite();
    tft.fillScreen(TFT_WHITE);
    tft.endWrite();

    while (playing) {
        unsigned long loopStart = millis();
        frame_count++;

        // --- ENTRADAS ---
        bool jump_p = (digitalRead(D_JUMP_BTN) == LOW || digitalRead(D_JUMP_ENC) == LOW || digitalRead(D_UP_BTN) == LOW);
        bool duck_p = (digitalRead(D_DOWN_BTN) == LOW);

        if (jump_p && !is_jumping) {
            d_v = -14.5; is_jumping = true;
            tone(D_BUZZER, 1200, 15);
        }
        is_ducking = duck_p;
        if (is_ducking && is_jumping) d_v += 2.2; 

        // --- FÍSICA ---
        if (is_jumping) {
            d_y += d_v; d_v += 1.0; 
            if (d_y >= 160) { d_y = 160; is_jumping = false; d_v = 0; }
        }

        // --- MOVIMIENTO E INCREMENTO ---
        cactus_x -= speed;
        if (cactus_x < -50) {
            cactus_x = 320 + random(0, 100);
            cactus_type = random(0, 3);
            score++; speed += 0.18; 
        }
        if (score >= 7) {
            bird_x -= (speed + 2.0);
            if (bird_x < -30) {
                bird_x = 400 + random(100, 400);
                if (abs(bird_x - cactus_x) < 160) bird_x += 160; 
            }
        }

        // --- COLISIONES ---
        if (cactus_x > 32 && cactus_x < 55 && d_y > 136) playing = false;
        if (bird_x > 32 && bird_x < 58) {
            if (!is_jumping && !is_ducking) playing = false;
            if (is_jumping && d_y > 115 && d_y < 155) playing = false;
        }

        // --- RENDER ---
        canvas->fillSprite(TFT_WHITE); 
        drawDetailedDino(*canvas, 40, (int)d_y, is_ducking, frame_count, is_jumping);
        drawRealCactus(*canvas, (int)cactus_x, 155, cactus_type);
        
        if (score >= 7) {
            int wing = (frame_count % 8 < 4) ? 0 : 8;
            canvas->fillRect(bird_x, 130, 20, 5, TFT_BLACK); 
            canvas->fillRect(bird_x + 5, 125 + wing, 4, 10, TFT_BLACK); 
        }
        
        canvas->drawFastHLine(0, 185, 320, TFT_BLACK); 
        canvas->setTextColor(TFT_BLACK); canvas->setTextSize(2);
        canvas->setCursor(10, 10); canvas->printf("SCORE: %04d", score);
        
        canvas->pushSprite(0, 0); 

        if (digitalRead(D_BACK) == LOW) { playing = false; }
        while (millis() - loopStart < 16) yield();
    }

    // --- GAME OVER ---
    tft.startWrite();
    tft.fillScreen(TFT_WHITE);
    tft.setTextColor(TFT_RED); tft.setTextSize(3);
    tft.setCursor(65, 100); tft.print("GAME OVER");
    tft.endWrite();
    tone(D_BUZZER, 100, 500);
    
    delay(800); 
    while(digitalRead(D_JUMP_BTN) == LOW || digitalRead(D_JUMP_ENC) == LOW) { yield(); }
    
    tft.fillScreen(TFT_BLACK); 
    delay(200);
    // El canvas NO se borra de la RAM para que la reentrada sea instantánea
}