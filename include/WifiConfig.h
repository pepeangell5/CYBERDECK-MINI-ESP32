#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  WIFI CONFIG · módulo reusable de conexión WiFi
//  · Guarda SSID/password en NVS (namespace "wificonfig")
//  · Intenta auto-conexión si hay credenciales
//  · UI de scan + selección + teclado virtual para password
//  · Reusable: cualquier herramienta puede llamar wifiConfigConnect()
// ═══════════════════════════════════════════════════════════════════════════

// Conecta a WiFi mostrando UI según sea necesario.
// Intenta credenciales guardadas primero. Si falla o no hay, abre UI.
//
// Returns:
//   true  = conectado (WiFi.status() == WL_CONNECTED)
//   false = usuario canceló o error irrecuperable
bool wifiConfigConnect();

// Borra credenciales guardadas en NVS (útil para "Olvidar red")
void wifiConfigForget();

// Retorna el SSID guardado en NVS, o "" si no hay
String wifiConfigGetSavedSSID();

// True si hay credenciales guardadas (no necesariamente válidas)
bool wifiConfigHasSaved();

#endif