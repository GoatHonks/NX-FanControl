#include <fancontrol.hpp>

namespace {
    bool         g_resolved = false;
    ConsoleModel g_model    = ConsoleModel_Unknown;
}

ConsoleModel GetConsoleModel(void) {
    if (g_resolved) {
        return g_model;
    }
    g_resolved = true;

    /* set:sys may already be up depending on the host process, so tolerate a
     * failed init and simply report Unknown. */
    const bool opened = R_SUCCEEDED(setsysInitialize());

    SetSysProductModel model = SetSysProductModel_Invalid;
    if (R_SUCCEEDED(setsysGetProductModel(&model))) {
        switch (model) {
            case SetSysProductModel_Nx:
            case SetSysProductModel_Copper:
                g_model = ConsoleModel_Erista;
                break;
            case SetSysProductModel_Iowa:
            case SetSysProductModel_Calcio:
                g_model = ConsoleModel_Mariko;
                break;
            case SetSysProductModel_Hoag:
                g_model = ConsoleModel_Lite;
                break;
            case SetSysProductModel_Aula:
                g_model = ConsoleModel_Oled;
                break;
            default:
                g_model = ConsoleModel_Unknown;
                break;
        }
    }

    if (opened) {
        setsysExit();
    }

    return g_model;
}

const char *GetConsoleModelName(void) {
    switch (GetConsoleModel()) {
        case ConsoleModel_Erista: return "Switch (V1)";
        case ConsoleModel_Mariko: return "Switch (V2)";
        case ConsoleModel_Lite:   return "Switch Lite";
        case ConsoleModel_Oled:   return "Switch OLED";
        default:                  return "Switch";
    }
}
