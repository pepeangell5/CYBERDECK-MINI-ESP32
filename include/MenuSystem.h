#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include "Icons.h"

// ═══════════════════════════════════════════════════════════════════════════
//  MENU SYSTEM · carrusel principal + submenús jerárquicos
//
//  Uso desde main.cpp:
//      runMainMenu();   // bucle infinito, jamás regresa
//
//  Uso desde tools que necesiten listas/submenús propios:
//      int sel = runSubMenu("WIFI TOOLS", items, 4);
//      if (sel == -1) return;   // usuario eligió BACK
//      switch (sel) { ... }
// ═══════════════════════════════════════════════════════════════════════════

// ── Entrada del carrusel principal ────────────────────────────────────────
struct MainMenuEntry {
    const char* title;           // Nombre de la categoría (ej: "WIFI TOOLS")
    const char* subtitle;        // Descripción corta (ej: "Scan, Deauth, ...")
    IconID      icon;            // ID del ícono a mostrar
    void        (*handler)();    // Función a llamar al presionar OK
};

// ── API pública ───────────────────────────────────────────────────────────

// Carrusel principal. Llámalo desde el loop() o setup() y no regresa.
void runMainMenu();

// Submenu tipo lista (scroll vertical). Devuelve:
//   · -1 si el usuario eligió BACK o mantuvo OK
//   · índice 0..count-1 del item seleccionado
int  runSubMenu(const char* title, const char* items[], int count);

#endif