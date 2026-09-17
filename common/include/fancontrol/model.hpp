#pragma once

#include <switch.h>

enum ConsoleModel {
    ConsoleModel_Unknown = 0,
    ConsoleModel_Erista,   /* V1 (Nx / Copper) */
    ConsoleModel_Mariko,   /* V2 (Iowa / Calcio) */
    ConsoleModel_Lite,     /* Hoag */
    ConsoleModel_Oled,     /* Aula */
};

/* Queries set:sys for the hardware model. Result is cached after the first
 * call, so this is cheap to call repeatedly. */
ConsoleModel GetConsoleModel(void);

/* Short display name, e.g. "Switch OLED". Never NULL. */
const char *GetConsoleModelName(void);
