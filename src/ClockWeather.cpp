#include "ClockWeather.h"
#include "WifiConfig.h"
#include "DisplayTFT.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "PepeDraw.h"
#include "Pins.h"
#include "SoundUtils.h"

extern DisplayTFT tft;

// ═══════════════════════════════════════════════════════════════════════════
//  CONFIG
// ═══════════════════════════════════════════════════════════════════════════
#define WEATHER_REFRESH_MS  600000UL    // 10 minutos

// NTP servers
static const char* NTP_SERVER_1 = "pool.ntp.org";
static const char* NTP_SERVER_2 = "time.google.com";

// Default fallback (Los Mochis, Sinaloa) si ip-api falla
#define FALLBACK_LAT       25.7894
#define FALLBACK_LON       -108.9956
#define FALLBACK_CITY      "Los Mochis"
#define FALLBACK_TZ_OFFSET (-7 * 3600)   // UTC-7 sin DST (Sinaloa)

// ═══════════════════════════════════════════════════════════════════════════
//  ESTADO
// ═══════════════════════════════════════════════════════════════════════════
static float    g_lat = FALLBACK_LAT;
static float    g_lon = FALLBACK_LON;
static String   g_city = FALLBACK_CITY;
static String   g_country = "MX";
static int      g_tzOffset = FALLBACK_TZ_OFFSET;
static String   g_timezone = "America/Mazatlan";   // IANA TZ

static float    g_tempC = 0;
static float    g_feelsLikeC = 0;
static int      g_humidity = 0;
static float    g_windKmh = 0;
static int      g_weatherCode = 0;
static String   g_sunrise = "";
static String   g_sunset = "";
static bool     g_isDay = true;

static unsigned long g_lastWeatherFetch = 0;
static unsigned long g_lastSecondTick = 0;



// ═══════════════════════════════════════════════════════════════════════════
//  IANA TIMEZONE → POSIX TZ STRING
//  Mapea los timezones más comunes (especialmente de Mexico) a sus
//  reglas POSIX correspondientes para configTzTime().
//  Si no encuentra match, retorna un genérico basado en offset.
// ═══════════════════════════════════════════════════════════════════════════
static String ianaToPosix(const String& iana, int offsetSec) {
    // Mexico (con DST sí/no según zona)
    if (iana == "America/Mexico_City")    return "CST6CDT,M4.1.0,M10.5.0";
    if (iana == "America/Cancun")         return "EST5";              // sin DST
    if (iana == "America/Merida")         return "CST6CDT,M4.1.0,M10.5.0";
    if (iana == "America/Monterrey")      return "CST6CDT,M4.1.0,M10.5.0";
    if (iana == "America/Matamoros")      return "CST6CDT,M3.2.0,M11.1.0";
    if (iana == "America/Mazatlan")       return "MST7";              // Sinaloa, sin DST
    if (iana == "America/Chihuahua")      return "MST7MDT,M4.1.0,M10.5.0";
    if (iana == "America/Ojinaga")        return "MST7MDT,M3.2.0,M11.1.0";
    if (iana == "America/Hermosillo")     return "MST7";              // Sonora, sin DST
    if (iana == "America/Tijuana")        return "PST8PDT,M3.2.0,M11.1.0";
    if (iana == "America/Bahia_Banderas") return "CST6CDT,M4.1.0,M10.5.0";

    // USA comunes
    if (iana == "America/Los_Angeles")    return "PST8PDT,M3.2.0,M11.1.0";
    if (iana == "America/Denver")         return "MST7MDT,M3.2.0,M11.1.0";
    if (iana == "America/Phoenix")        return "MST7";              // Arizona, sin DST
    if (iana == "America/Chicago")        return "CST6CDT,M3.2.0,M11.1.0";
    if (iana == "America/New_York")       return "EST5EDT,M3.2.0,M11.1.0";

    // Otros comunes en habla hispana
    if (iana == "America/Bogota")         return "COT5";
    if (iana == "America/Lima")           return "PET5";
    if (iana == "America/Santiago")       return "CLT4CLST,M9.1.6/24,M4.1.6/24";
    if (iana == "America/Buenos_Aires")   return "ART3";
    if (iana == "Europe/Madrid")          return "CET-1CEST,M3.5.0,M10.5.0/3";

    // Fallback: armar string genérico desde el offset (sin DST)
    int hours = -offsetSec / 3600;   // signo invertido en POSIX
    char buf[16];
    if (hours >= 0) snprintf(buf, sizeof(buf), "UTC%d", hours);
    else            snprintf(buf, sizeof(buf), "UTC+%d", -hours);
    return String(buf);
}


// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS DE TIEMPO
// ═══════════════════════════════════════════════════════════════════════════

static const char* DAYS_ES[] = {
    "Domingo", "Lunes", "Martes", "Miercoles",
    "Jueves", "Viernes", "Sabado"
};

static const char* MONTHS_ES[] = {
    "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
    "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
};

static String formatHHMMSS(struct tm* t) {
    int h = t->tm_hour;
    int displayH;
    if (h == 0)        displayH = 12;       // medianoche
    else if (h > 12)   displayH = h - 12;
    else               displayH = h;

    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             displayH, t->tm_min, t->tm_sec);
    return String(buf);
}

// Helper que retorna "AM" o "PM" según la hora
static String getAmPm(struct tm* t) {
    return (t->tm_hour < 12) ? "AM" : "PM";
}

static String formatHHMM(struct tm* t) {
    int h = t->tm_hour;
    int displayH;
    if (h == 0)        displayH = 12;
    else if (h > 12)   displayH = h - 12;
    else               displayH = h;

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", displayH, t->tm_min);
    return String(buf);
}

static String formatDate(struct tm* t) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%s, %d de %s",
             DAYS_ES[t->tm_wday], t->tm_mday, MONTHS_ES[t->tm_mon]);
    return String(buf);
}

// ═══════════════════════════════════════════════════════════════════════════
//  ICONOS DEL CLIMA (pixel art 32x32)
//  Códigos WMO de Open-Meteo:
//    0       = clear sky
//    1,2,3   = mainly clear, partly cloudy, overcast
//    45,48   = fog
//    51-67   = drizzle, rain
//    71-77   = snow
//    80-82   = rain showers
//    95-99   = thunderstorm
// ═══════════════════════════════════════════════════════════════════════════

// Categorización del weather code
enum WeatherIcon {
    ICON_SUN,
    ICON_PARTLY_CLOUDY,
    ICON_CLOUDY,
    ICON_RAIN,
    ICON_THUNDER,
    ICON_SNOW,
    ICON_FOG
};

static WeatherIcon weatherCodeToIcon(int code) {
    if (code == 0) return ICON_SUN;
    if (code <= 2) return ICON_PARTLY_CLOUDY;
    if (code == 3) return ICON_CLOUDY;
    if (code == 45 || code == 48) return ICON_FOG;
    if (code >= 51 && code <= 67) return ICON_RAIN;
    if (code >= 71 && code <= 77) return ICON_SNOW;
    if (code >= 80 && code <= 82) return ICON_RAIN;
    if (code >= 95) return ICON_THUNDER;
    return ICON_CLOUDY;
}

static String weatherCodeToDescES(int code) {
    if (code == 0) return "Despejado";
    if (code == 1) return "Mayormente despejado";
    if (code == 2) return "Parcialmente nublado";
    if (code == 3) return "Nublado";
    if (code == 45 || code == 48) return "Niebla";
    if (code == 51 || code == 53 || code == 55) return "Llovizna";
    if (code == 61 || code == 63) return "Lluvia ligera";
    if (code == 65) return "Lluvia fuerte";
    if (code == 71 || code == 73 || code == 75) return "Nieve";
    if (code == 80 || code == 81) return "Chubascos";
    if (code == 82) return "Chubascos fuertes";
    if (code == 95) return "Tormenta";
    if (code >= 96) return "Tormenta granizo";
    return "Desconocido";
}

// Dibuja sol
static void drawSunIcon(int cx, int cy, int size, uint16_t color) {
    int r = size / 4;
    tft.fillCircle(cx, cy, r, color);
    // Rayos
    for (int a = 0; a < 8; a++) {
        float ang = a * 45.0 * PI / 180.0;
        int x1 = cx + (int)(cos(ang) * (r + 3));
        int y1 = cy + (int)(sin(ang) * (r + 3));
        int x2 = cx + (int)(cos(ang) * (r + 8));
        int y2 = cy + (int)(sin(ang) * (r + 8));
        tft.drawLine(x1, y1, x2, y2, color);
    }
}

// Dibuja nube
static void drawCloudIcon(int cx, int cy, uint16_t color) {
    tft.fillCircle(cx - 8, cy + 2, 7, color);
    tft.fillCircle(cx + 6, cy + 2, 8, color);
    tft.fillCircle(cx - 2, cy - 4, 9, color);
    tft.fillRect(cx - 12, cy + 2, 22, 6, color);
}

// Dibuja gotas de lluvia
static void drawRainDrops(int cx, int cy, uint16_t color) {
    for (int i = -1; i <= 1; i++) {
        int x = cx + i * 6;
        int y = cy + 12;
        tft.drawLine(x, y, x - 2, y + 6, color);
        tft.drawPixel(x - 1, y + 7, color);
    }
}

// Dibuja rayo
static void drawLightning(int cx, int cy, uint16_t color) {
    tft.drawLine(cx - 2, cy + 5, cx, cy + 12, color);
    tft.drawLine(cx, cy + 12, cx - 3, cy + 12, color);
    tft.drawLine(cx - 3, cy + 12, cx + 1, cy + 18, color);
    tft.drawLine(cx + 1, cy + 18, cx + 3, cy + 14, color);
}

// Dibuja copos de nieve
static void drawSnowflakes(int cx, int cy, uint16_t color) {
    for (int i = -1; i <= 1; i++) {
        int x = cx + i * 8;
        int y = cy + 14;
        tft.drawPixel(x, y, color);
        tft.drawPixel(x - 1, y, color);
        tft.drawPixel(x + 1, y, color);
        tft.drawPixel(x, y - 1, color);
        tft.drawPixel(x, y + 1, color);
    }
}

static void drawWeatherIcon(int cx, int cy, WeatherIcon icon) {
    switch (icon) {
        case ICON_SUN:
            drawSunIcon(cx, cy, 32, TFT_YELLOW);
            break;
        case ICON_PARTLY_CLOUDY:
            drawSunIcon(cx - 8, cy - 4, 26, TFT_YELLOW);
            drawCloudIcon(cx + 6, cy + 4, UI_MAIN);
            break;
        case ICON_CLOUDY:
            drawCloudIcon(cx, cy, UI_MAIN);
            break;
        case ICON_RAIN:
            drawCloudIcon(cx, cy - 5, UI_ACCENT);
            drawRainDrops(cx, cy, TFT_CYAN);
            break;
        case ICON_THUNDER:
            drawCloudIcon(cx, cy - 5, UI_ACCENT);
            drawLightning(cx, cy, TFT_YELLOW);
            break;
        case ICON_SNOW:
            drawCloudIcon(cx, cy - 5, UI_MAIN);
            drawSnowflakes(cx, cy, TFT_WHITE);
            break;
        case ICON_FOG:
            tft.drawFastHLine(cx - 14, cy - 4, 28, UI_ACCENT);
            tft.drawFastHLine(cx - 16, cy, 32, UI_ACCENT);
            tft.drawFastHLine(cx - 12, cy + 4, 24, UI_ACCENT);
            tft.drawFastHLine(cx - 14, cy + 8, 28, UI_ACCENT);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  IP GEOLOCATION (ip-api.com)
// ═══════════════════════════════════════════════════════════════════════════

static bool fetchGeolocation() {
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin("http://ip-api.com/json/?fields=status,country,city,lat,lon,timezone,offset")) {
        return false;
    }

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return false;

    if (doc["status"] != "success") return false;

    g_lat = doc["lat"].as<float>();
    g_lon = doc["lon"].as<float>();
    g_city = doc["city"].as<String>();
    g_country = doc["country"].as<String>();

    // offset viene en segundos
    if (!doc["offset"].isNull()) {
        g_tzOffset = doc["offset"].as<int>();
    }

    // IANA timezone (ej: "America/Mazatlan")
    if (!doc["timezone"].isNull()) {
        g_timezone = doc["timezone"].as<String>();
    }

    return true;
}
// ═══════════════════════════════════════════════════════════════════════════
//  WEATHER FETCH (Open-Meteo)
// ═══════════════════════════════════════════════════════════════════════════

static bool fetchWeather() {
    HTTPClient http;
    http.setTimeout(10000);

    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast"
             "?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
             "is_day,weather_code,wind_speed_10m"
             "&daily=sunrise,sunset"
             "&timezone=auto&forecast_days=1",
             g_lat, g_lon);

    if (!http.begin(url)) return false;

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return false;

    JsonObject current = doc["current"];
    g_tempC        = current["temperature_2m"].as<float>();
    g_feelsLikeC   = current["apparent_temperature"].as<float>();
    g_humidity     = current["relative_humidity_2m"].as<int>();
    g_windKmh      = current["wind_speed_10m"].as<float>();
    g_weatherCode  = current["weather_code"].as<int>();
    g_isDay        = current["is_day"].as<int>() == 1;

    // Sunrise/sunset format: "2026-04-25T06:23"
    String sr = doc["daily"]["sunrise"][0].as<String>();
    String ss = doc["daily"]["sunset"][0].as<String>();
    if (sr.length() >= 16) g_sunrise = sr.substring(11, 16);
    if (ss.length() >= 16) g_sunset = ss.substring(11, 16);

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  NTP SYNC
// ═══════════════════════════════════════════════════════════════════════════

static bool syncNTP() {
    // Mapear IANA timezone a POSIX TZ string para manejar DST correctamente
    String posixTz = ianaToPosix(g_timezone, g_tzOffset);

    // configTzTime usa POSIX TZ → respeta DST automáticamente según las reglas
    configTzTime(posixTz.c_str(), NTP_SERVER_1, NTP_SERVER_2);

    struct tm timeinfo;
    int retries = 0;
    while (!getLocalTime(&timeinfo, 1500) && retries < 10) {
        retries++;
        delay(500);
    }
    return retries < 10;
}

// ═══════════════════════════════════════════════════════════════════════════
//  PANTALLA DE LOADING
// ═══════════════════════════════════════════════════════════════════════════

static void drawLoadingStep(const String& step, int progress) {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);
    drawStringBig(40, 20, "CLOCK & WEATHER", UI_MAIN, 1);
    tft.drawFastHLine(0, 50, 320, UI_ACCENT);

    drawStringCustom(20, 100, "Cargando...", UI_ACCENT, 1);
    drawStringBig(20, 120, step, UI_SELECT, 1);

    int barX = 20, barY = 180, barW = 280, barH = 14;
    tft.drawRect(barX, barY, barW, barH, UI_ACCENT);
    int fw = (progress * (barW - 2)) / 100;
    tft.fillRect(barX + 1, barY + 1, fw, barH - 2, UI_SELECT);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PANTALLA PRINCIPAL: RELOJ + CLIMA
// ═══════════════════════════════════════════════════════════════════════════

static void drawMainScreenFrame() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    // City + day indicator (sun/moon)
    drawStringCustom(10, 8, g_city + ", " + g_country, UI_ACCENT, 1);
    drawStringCustom(260, 8, g_isDay ? "DIA" : "NOCHE", UI_ACCENT, 1);
    tft.drawFastHLine(0, 22, 320, UI_ACCENT);

    // Separador entre reloj y clima
    tft.drawFastHLine(0, 130, 320, UI_ACCENT);

    // Footer
    tft.drawFastHLine(0, 220, 320, UI_ACCENT);
    drawStringCustom(10, 226, "OK(HOLD): EXIT", UI_ACCENT, 1);
}

static void drawClock(struct tm* t) {
    // Borrar área del reloj
    tft.fillRect(2, 24, 316, 105, TFT_BLACK);

    // Hora gigante centrada
    String timeStr = formatHHMMSS(t);
    int timeW = getTextWidth(timeStr, 4, FONT_BIG);

    // AM/PM al lado en tamaño menor
    String ampmStr = getAmPm(t);
    int ampmW = getTextWidth(ampmStr, 2, FONT_BIG);

    // Centrado considerando hora + espacio + AM/PM
    int totalW = timeW + 8 + ampmW;
    int timeX = (320 - totalW) / 2;
    int ampmX = timeX + timeW + 8;

    drawStringBig(timeX, 35, timeStr, UI_MAIN, 4);
    // AM/PM con color distinto y un poco más abajo (alineado al baseline)
    uint16_t ampmColor = (t->tm_hour < 12) ? TFT_CYAN : TFT_ORANGE;
    drawStringBig(ampmX, 55, ampmStr, ampmColor, 2);

    // Fecha abajo
    String dateStr = formatDate(t);
    int dateW = getTextWidth(dateStr, 1, FONT_SMALL);
    int dateX = (320 - dateW) / 2;
    if (dateX < 5) dateX = 5;
    drawStringCustom(dateX, 100, dateStr, UI_ACCENT, 1);

    // Año
    char yearBuf[8];
    snprintf(yearBuf, sizeof(yearBuf), "%d", t->tm_year + 1900);
    int yearW = getTextWidth(String(yearBuf), 1, FONT_SMALL);
    drawStringCustom((320 - yearW) / 2, 115, String(yearBuf), UI_ACCENT, 1);
}

static void drawWeather() {
    // Borrar área del clima
    tft.fillRect(2, 132, 316, 86, TFT_BLACK);

    // Icono del clima a la izquierda
    WeatherIcon icon = weatherCodeToIcon(g_weatherCode);
    drawWeatherIcon(48, 168, icon);

    // Temperatura grande al centro-derecha
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.0fC", g_tempC);
    drawStringBig(110, 145, String(tempBuf), TFT_YELLOW, 3);

    // Sensación térmica
    char feelsBuf[24];
    snprintf(feelsBuf, sizeof(feelsBuf), "Sensacion: %.0fC", g_feelsLikeC);
    drawStringCustom(110, 178, String(feelsBuf), UI_MAIN, 1);

    // Descripción del clima
    String desc = weatherCodeToDescES(g_weatherCode);
    drawStringCustom(110, 192, desc, UI_ACCENT, 1);

    // Humedad y viento (lado derecho)
    char humBuf[16];
    snprintf(humBuf, sizeof(humBuf), "%d%% hum", g_humidity);
    drawStringCustom(245, 145, String(humBuf), TFT_CYAN, 1);

    char windBuf[20];
    snprintf(windBuf, sizeof(windBuf), "%.0f km/h", g_windKmh);
    drawStringCustom(245, 159, String(windBuf), TFT_CYAN, 1);

    // Sunrise / sunset
    if (g_sunrise.length() > 0) {
        drawStringCustom(245, 178, "^ " + g_sunrise, TFT_ORANGE, 1);
    }
    if (g_sunset.length() > 0) {
        drawStringCustom(245, 192, "v " + g_sunset, UI_SELECT, 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════════

static void mainLoop() {
    drawMainScreenFrame();

    // Initial draws
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        drawClock(&timeinfo);
    }
    drawWeather();

    g_lastWeatherFetch = millis();
    g_lastSecondTick = millis();

    bool stop = false;
    unsigned long okPressStart = 0;
    bool okHeld = false;

    while (!stop) {
        // Update clock cada segundo
        if (millis() - g_lastSecondTick >= 1000) {
            if (getLocalTime(&timeinfo)) {
                drawClock(&timeinfo);
            }
            g_lastSecondTick = millis();
        }

        // Update weather cada 10 minutos
        if (millis() - g_lastWeatherFetch >= WEATHER_REFRESH_MS) {
            // Mini indicator
            tft.fillRect(305, 8, 10, 10, TFT_YELLOW);
            if (fetchWeather()) {
                drawWeather();
                tft.fillRect(305, 8, 10, 10, TFT_GREEN);
                delay(500);
                tft.fillRect(305, 8, 10, 10, TFT_BLACK);
            } else {
                tft.fillRect(305, 8, 10, 10, TFT_RED);
            }
            g_lastWeatherFetch = millis();
        }

        // OK hold para salir
        if (navEnterPressed()) {
            if (!okHeld) {
                okPressStart = millis();
                okHeld = true;
            } else if (millis() - okPressStart > 500) {
                stop = true;
            }
        } else {
            okHeld = false;
        }

        delay(50);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════

void runClockWeather() {
    while (navEnterPressed()) delay(5);
    delay(100);

    // 1. Conectar WiFi (módulo reusable)
    drawLoadingStep("Conectando WiFi...", 5);
    delay(500);

    if (!wifiConfigConnect()) {
        // Usuario canceló o falló
        return;
    }

    // 2. IP geolocation
    drawLoadingStep("Detectando ubicacion...", 30);
    bool geoOk = fetchGeolocation();
    if (!geoOk) {
        // Usar fallback
        g_lat = FALLBACK_LAT;
        g_lon = FALLBACK_LON;
        g_city = FALLBACK_CITY;
        g_country = "MX";
        g_tzOffset = FALLBACK_TZ_OFFSET;
    }

    // 3. NTP sync
    drawLoadingStep("Sincronizando hora...", 55);
    if (!syncNTP()) {
        tft.fillScreen(TFT_BLACK);
        tft.drawRect(0, 0, 320, 240, TFT_RED);
        drawStringBig(50, 90, "NTP FALLO", TFT_RED, 2);
        drawStringCustom(40, 130, "No se pudo sincronizar la hora.",
                         UI_MAIN, 1);
        drawStringCustom(40, 220, "OK: salir", UI_MAIN, 1);
        beep(800, 100);
        while (!navEnterPressed()) delay(20);
        while (navEnterPressed()) delay(5);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return;
    }

    // 4. Weather
    drawLoadingStep("Obteniendo clima...", 80);
    if (!fetchWeather()) {
        // Default values si falla
        g_tempC = 0;
        g_humidity = 0;
        g_windKmh = 0;
        g_weatherCode = 0;
        g_sunrise = "06:00";
        g_sunset = "19:00";
    }

    drawLoadingStep("Listo!", 100);
    beep(2400, 50); delay(30);
    beep(3000, 50); delay(30);
    beep(3600, 80);
    delay(500);

    // 5. Loop principal
    mainLoop();

    // Cleanup
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    beep(1800, 40); delay(20);
    beep(1200, 60);

    while (navEnterPressed()) delay(5);
    delay(150);
}
