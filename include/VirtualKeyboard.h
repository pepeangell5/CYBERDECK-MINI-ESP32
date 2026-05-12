#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  TECLADO VIRTUAL · 10 columnas × 4 filas alfanuméricas + 5 especiales
//  · Navegación: UP/DOWN columna por columna (vertical primero)
//  · SHIFT toggle mayúsculas
//  · Layout QWERTY español con ñ
//  · Retorna el string ingresado o "" si se canceló
// ═══════════════════════════════════════════════════════════════════════════

// Muestra el teclado y retorna el string ingresado por el usuario.
// Si el usuario cancela (X), retorna string vacío.
//
// Parameters:
//   title:       título mostrado en la parte superior (ej: "WIFI PASSWORD")
//   subtitle:    línea bajo el título (ej: "Red: MiCasa_Telmex")
//   maxLen:      máximo de caracteres permitidos (62 default)
//   maskInput:   true = muestra asteriscos (***), false = muestra texto plano
//
// Returns:
//   String ingresado, o "" si se canceló
String virtualKeyboardInput(const String& title,
                             const String& subtitle,
                             int maxLen = 62,
                             bool maskInput = false);

#endif