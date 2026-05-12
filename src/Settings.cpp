#include "Settings.h"

// ═══════════════════════════════════════════════════════════════════════════
//  Variables globales de configuración del usuario.
//
//  IMPORTANTE: estos valores "iniciales" son solo fallback.
//  El valor real se carga desde NVS en setup() mediante loadSettings().
//  Cualquier cambio durante runtime debe guardarse con saveSettings().
// ═══════════════════════════════════════════════════════════════════════════

bool soundEnabled = true;
int  soundVolume  = 3;   // rango 1-5