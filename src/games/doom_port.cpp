#include "games/doom_port.h"
#include <math.h>

#define MAP_W 12
#define MAP_H 12
int worldMap[MAP_W][MAP_H] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,0,0,1,1,1,0,1},
    {1,0,1,0,0,0,0,0,0,1,0,1},
    {1,0,1,0,1,1,1,1,0,1,0,1},
    {1,0,0,0,1,0,0,1,0,0,0,1},
    {1,0,1,0,1,0,0,1,0,1,0,1},
    {1,0,1,0,1,1,1,1,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,0,1,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1}
};

void launchDoom(LGFX &tft, bool &playing, int btnBack) {
    float posX = 3.0, posY = 3.0; 
    float dirX = -1.0, dirY = 0.0; 
    float planeX = 0.0, planeY = 0.66; 

    // Pines de control PepeAngell
    const int B_UP = 1, B_DOWN = 2, B_ENTER = 42, B_BACK = 41, E_SW = 3;
    const int E_CLK = 48, E_DT = 46;
    
    int lastClk = digitalRead(E_CLK);

    // Buffer de video (Sprite)
    LGFX_Sprite canvas(&tft);
    canvas.createSprite(160, 120); 

    while (playing) {
        // --- 1. VERIFICACIÓN DE SALIDA (AL PRINCIPIO DEL CICLO) ---
        if (digitalRead(B_BACK) == LOW || digitalRead(E_SW) == LOW) {
            canvas.deleteSprite(); // Liberar RAM de inmediato
            playing = false;
            while(digitalRead(B_BACK) == LOW || digitalRead(E_SW) == LOW) yield(); // Esperar a que suelte
            return; // Salida forzosa
        }

        // --- 2. RENDERIZADO AL CANVAS ---
        for (int x = 0; x < 160; x++) { 
            float cameraX = 2 * x / (float)160 - 1;
            float rayDirX = dirX + planeX * cameraX;
            float rayDirY = dirY + planeY * cameraX;
            int mapX = int(posX), mapY = int(posY);
            float deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1 / rayDirX);
            float deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1 / rayDirY);
            float sideDistX, sideDistY;
            int stepX, stepY, hit = 0, side;

            if (rayDirX < 0) { stepX = -1; sideDistX = (posX - mapX) * deltaDistX; }
            else { stepX = 1; sideDistX = (mapX + 1.0 - posX) * deltaDistX; }
            if (rayDirY < 0) { stepY = -1; sideDistY = (posY - mapY) * deltaDistY; }
            else { stepY = 1; sideDistY = (mapY + 1.0 - posX) * deltaDistY; }

            while (hit == 0) {
                if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
                else { sideDistY += deltaDistY; mapY += stepY; side = 1; }
                if (worldMap[mapX][mapY] > 0) hit = 1;
            }

            float perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
            int lineHeight = (int)(120 / perpWallDist);
            int drawStart = -lineHeight / 2 + 60;
            if (drawStart < 0) drawStart = 0;
            int drawEnd = lineHeight / 2 + 60;
            if (drawEnd >= 120) drawEnd = 119;

            uint16_t wallColor = (side == 1) ? 0xAD55 : TFT_WHITE; 
            canvas.drawFastVLine(x, 0, drawStart, 0x2104);
            canvas.drawFastVLine(x, drawStart, drawEnd - drawStart, wallColor);
            canvas.drawFastVLine(x, drawEnd, 120 - drawEnd, 0x1082);
        }

        // Mira y Empuje a pantalla
        canvas.drawFastHLine(75, 60, 10, TFT_RED);
        canvas.drawFastVLine(80, 55, 10, TFT_RED);
        canvas.drawCircle(80, 60, 3, TFT_RED);
        canvas.pushRotateZoom(&tft, 160, 120, 0, 2, 2); 

        // --- 3. MOVIMIENTO Y DISPARO ---
        float moveSpeed = 0.12; 
        float rotSpeed = 0.15; 

        if (digitalRead(B_UP) == LOW) {
            if(worldMap[int(posX + dirX * moveSpeed)][int(posY)] == 0) posX += dirX * moveSpeed;
            if(worldMap[int(posX)][int(posY + dirY * moveSpeed)] == 0) posY += dirY * moveSpeed;
        }
        if (digitalRead(B_DOWN) == LOW) {
            if(worldMap[int(posX - dirX * moveSpeed)][int(posY)] == 0) posX -= dirX * moveSpeed;
            if(worldMap[int(posX)][int(posY - dirY * moveSpeed)] == 0) posY -= dirY * moveSpeed;
        }

        if (digitalRead(B_ENTER) == LOW) {
            digitalWrite(14, HIGH); 
            tft.fillCircle(160, 120, 50, TFT_RED); 
            delay(40);
            digitalWrite(14, LOW);
            while(digitalRead(B_ENTER) == LOW) yield();
        }

        // --- 4. ENCODER ---
        int currentClk = digitalRead(E_CLK);
        if (currentClk != lastClk) {
            if (currentClk == LOW) {
                int dtVal = digitalRead(E_DT);
                float angle = (dtVal != currentClk) ? rotSpeed : -rotSpeed;
                float oldDirX = dirX;
                dirX = dirX * cos(angle) - dirY * sin(angle);
                dirY = oldDirX * sin(angle) + dirY * cos(angle);
                float oldPlaneX = planeX;
                planeX = planeX * cos(angle) - planeY * sin(angle);
                planeY = oldPlaneX * sin(angle) + planeY * cos(angle);
            }
            delay(2); 
        }
        lastClk = currentClk;
        
        yield();
    }
}