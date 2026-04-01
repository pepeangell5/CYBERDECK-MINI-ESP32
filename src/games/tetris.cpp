#include "games/tetris.h"

// Pines Cyberdeck PepeAngell
const int T_UP     = 1;  // MOVER IZQUIERDA
const int T_DOWN   = 2;  // MOVER DERECHA
const int T_OK     = 42; // ROTAR PIEZA
const int T_BACK   = 41; // SALIR DEL JUEGO
const int T_ENC_SW = 3;  // CLICK ENCODER: CAÍDA RÁPIDA
const int T_BUZZER = 14;

// Configuración Tablero
const int B_W = 10;
const int B_H = 20;
const int B_SIZE = 11; 
static int tetris_board[10][20];

// Piezas (Tetrominos)
const uint16_t t_shapes[7][4] = {
    {0x4444, 0x0F00, 0x4444, 0x0F00}, {0x4460, 0x0E80, 0xC440, 0x2E00},
    {0x44C0, 0x8E00, 0x6440, 0x0E20}, {0x0660, 0x0660, 0x0660, 0x0660},
    {0x06C0, 0x8C40, 0x06C0, 0x8C40}, {0x0E40, 0x4C40, 0x4E00, 0x4640},
    {0x0C60, 0x4C80, 0x0C60, 0x4C80}
};
const uint16_t t_colors[] = {TFT_CYAN, TFT_ORANGE, TFT_BLUE, TFT_YELLOW, TFT_GREEN, TFT_MAGENTA, TFT_RED};

static int cX, cY, cType, cRot, t_score;
static LGFX_Sprite* t_canvas = nullptr;

// --- LÓGICA DE JUEGO ---

bool checkTetrisCollision(int x, int y, int r) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if ((t_shapes[cType][r] >> (15 - (i + j * 4))) & 1) {
                int nx = x + i; int ny = y + j;
                if (nx < 0 || nx >= B_W || ny >= B_H || (ny >= 0 && tetris_board[nx][ny])) return true;
            }
        }
    }
    return false;
}

void lockTetrisPiece() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if ((t_shapes[cType][cRot] >> (15 - (i + j * 4))) & 1) {
                if (cY + j >= 0) tetris_board[cX + i][cY + j] = t_colors[cType];
            }
        }
    }
    for (int j = B_H - 1; j >= 0; j--) {
        bool full = true;
        for (int i = 0; i < B_W; i++) if (!tetris_board[i][j]) full = false;
        if (full) {
            t_score += 100; tone(T_BUZZER, 800, 40);
            for (int k = j; k > 0; k--) for (int i = 0; i < B_W; i++) tetris_board[i][k] = tetris_board[i][k - 1];
            j++;
        }
    }
}

void spawnTetrisPiece() {
    cX = B_W / 2 - 2; cY = -2; cType = random(7); cRot = 0;
}

void launchTetris(LGFX &tft) {
    if (t_canvas == nullptr) {
        t_canvas = new LGFX_Sprite(&tft);
        t_canvas->createSprite(320, 240);
        t_canvas->setColorDepth(8);
    }

    memset(tetris_board, 0, sizeof(tetris_board));
    t_score = 0; spawnTetrisPiece();
    unsigned long lastDrop = millis();
    bool playing = true;

    while (playing) {
        unsigned long now = millis();

        // --- CONTROLES FINAL ---

        // IZQUIERDA (Botón 1)
        if (digitalRead(T_UP) == LOW) {
            if (!checkTetrisCollision(cX - 1, cY, cRot)) cX--;
            delay(110); 
        }

        // DERECHA (Botón 2)
        if (digitalRead(T_DOWN) == LOW) {
            if (!checkTetrisCollision(cX + 1, cY, cRot)) cX++;
            delay(110);
        }

        // ROTAR (Botón 42)
        if (digitalRead(T_OK) == LOW) {
            int nextR = (cRot + 1) % 4;
            if (!checkTetrisCollision(cX, cY, nextR)) {
                cRot = nextR;
                tone(T_BUZZER, 1100, 10);
            }
            delay(180); 
        }

        // CAÍDA RÁPIDA (Click Encoder - Botón 3)
        int fallInterval = 500;
        if (digitalRead(T_ENC_SW) == LOW) {
            fallInterval = 35; // Caída súper rápida
        }

        // SALIR (Botón 41)
        if (digitalRead(T_BACK) == LOW) {
            playing = false;
            delay(250); // Delay preventivo para no salir del menú principal también
        }

        // --- FÍSICA ---
        if (now - lastDrop > (unsigned long)fallInterval) {
            if (!checkTetrisCollision(cX, cY + 1, cRot)) {
                cY++;
            } else {
                if (cY < 0) playing = false; // Game Over
                else { lockTetrisPiece(); spawnTetrisPiece(); }
            }
            lastDrop = now;
        }

        // --- RENDER ---
        t_canvas->fillSprite(TFT_BLACK);
        
        // Área de juego (Color Matrix)
        t_canvas->drawRect(108, 13, B_W * B_SIZE + 4, B_H * B_SIZE + 4, 0x07E0); 
        
        for (int i = 0; i < B_W; i++) {
            for (int j = 0; j < B_H; j++) {
                if (tetris_board[i][j]) 
                    t_canvas->fillRect(110 + i * B_SIZE, 15 + j * B_SIZE, B_SIZE - 1, B_SIZE - 1, tetris_board[i][j]);
            }
        }

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if ((t_shapes[cType][cRot] >> (15 - (i + j * 4))) & 1) {
                    if (cY + j >= 0)
                        t_canvas->fillRect(110 + (cX + i) * B_SIZE, 15 + (cY + j) * B_SIZE, B_SIZE - 1, B_SIZE - 1, t_colors[cType]);
                }
            }
        }

        // UI
        t_canvas->setTextColor(0x07E0); t_canvas->setTextSize(2);
        t_canvas->setCursor(10, 20); t_canvas->print("TETRIS");
        t_canvas->setCursor(10, 60); t_canvas->printf("SCORE:\n%05d", t_score);
        
        t_canvas->setTextSize(1);
        t_canvas->setCursor(10, 180); 
        t_canvas->print("UP: LEFT\nDWN: RIGHT\nOK: ROTATE\nENC: FAST DROP\nBACK: EXIT");
        
        t_canvas->pushSprite(0, 0);
        yield();
    }

    t_canvas->fillSprite(TFT_BLACK);
    t_canvas->setTextColor(TFT_RED); t_canvas->setTextSize(3);
    t_canvas->setCursor(65, 100); t_canvas->print("GAME OVER");
    t_canvas->pushSprite(0, 0);
    delay(1500);
}