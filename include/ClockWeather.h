#ifndef CLOCK_WEATHER_H
#define CLOCK_WEATHER_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  CLOCK & WEATHER
//  · NTP sync para hora exacta
//  · IP geolocation (ip-api.com) para detectar ciudad
//  · Open-Meteo API para datos climáticos (sin API key)
//  · Pantalla con reloj grande + clima con iconos pixel art
// ═══════════════════════════════════════════════════════════════════════════

void runClockWeather();

#endif