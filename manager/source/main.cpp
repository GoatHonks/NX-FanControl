/* NXFanControl Manager
 *
 * The full editor for NX-FanControl. The Tesla overlay is the quick in-game
 * companion; this is the main app, and it can do everything the overlay can
 * plus naming and reordering, which need the system keyboard the overlay
 * cannot reach.
 *
 * Both write the same config.ini. The sysmodule watches that file's mtime and
 * hot-reloads, so edits here take effect within a couple of seconds.
 */

#include <switch.h>
#include <fancontrol.hpp>

#include <SDL2/SDL.h>

#include "app.hpp"
#include "sensors.hpp"
#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

    /* ---- layout ---- */
    constexpr int Margin      = 40;
    constexpr int HeaderH     = 92;
    constexpr int TabBarY     = HeaderH;
    constexpr int TabBarH     = 54;
    constexpr int ContentY    = TabBarY + TabBarH + 18;
    constexpr int FooterY     = ui::ScreenH - 62;
    constexpr int ContentH    = FooterY - ContentY - 14;

    constexpr int ListW       = 436;
    constexpr int PanelX      = Margin + ListW + 20;
    constexpr int PanelW      = ui::ScreenW - PanelX - Margin;

    const char *TabNames[Tab_Count] = { "Profiles", "Fan Curve", "Games", "Settings" };

    /* ---- input repeat ---- */
    struct Repeat {
        u64 nextTick = 0;
        u64 heldKey  = 0;

        bool fire(u64 keysDown, u64 keysHeld, u64 mask) {
            const u64 now  = armGetSystemTick();
            const u64 freq = armGetSystemTickFreq();

            if (keysDown & mask) {
                this->heldKey = mask;
                this->nextTick = now + freq / 2;   /* initial delay */
                return true;
            }
            if ((keysHeld & mask) && this->heldKey == mask && now >= this->nextTick) {
                this->nextTick = now + freq / 14;  /* repeat rate */
                return true;
            }
            if (!(keysHeld & mask) && this->heldKey == mask) {
                this->heldKey = 0;
            }
            return false;
        }
    };

    Repeat g_repUp, g_repDown, g_repLeft, g_repRight, g_repL, g_repR;

    /* Curve shown on the Profiles tab: the highlighted profile, which is not
     * necessarily the active one. */
    CurveBuffer g_preview;
    int         g_previewFor = -1;

    /* Pending settings writes, flushed shortly after the last change so that
     * holding a direction does not rewrite the config on every step. */
    bool g_settingsDirty = false;
    u64  g_settingsFlushAt = 0;

    enum Modal {
        Modal_None = 0,
        Modal_DeleteProfile,
        Modal_PresetPicker,
        Modal_UnbindGame,
    };

    Modal g_modal = Modal_None;

    /* Preset picker: chosen after the name is entered, so a new profile can
     * start from a copy or from a built-in template. */
    int  g_presetCursor = 0;
    char g_pendingName[MaxProfileNameLength + 1] = {};

    struct PresetChoice {
        const char *name;
        const char *help;
        int         preset;   /* -1 = copy the highlighted profile */
    };

    const PresetChoice PresetChoices[] = {
        { "Copy highlighted profile", "Start from the curves you already have", -1 },
        { "Default",                  "Silent until warm, then ramps",          CurvePreset_Default },
        { "Stock-like",               "Approximates Nintendo's stock behaviour", CurvePreset_StockLike },
    };
    constexpr int PresetChoiceCount = sizeof(PresetChoices) / sizeof(PresetChoices[0]);

    void MarkSettingsDirty() {
        g_settingsDirty = true;
        g_settingsFlushAt = armGetSystemTick() + armGetSystemTickFreq();
    }

    void FlushSettingsIfDue(bool force) {
        if (!g_settingsDirty) {
            return;
        }
        if (!force && armGetSystemTick() < g_settingsFlushAt) {
            return;
        }
        g_app.saveSettings();
        g_settingsDirty = false;
    }

    void RefreshPreview() {
        if (g_app.profileCount == 0) {
            return;
        }
        const u32 id = g_app.profileIds[g_app.profileCursor];
        if ((int)id == g_previewFor) {
            return;
        }
        g_preview.bind(id, false);
        g_preview.load();
        g_previewFor = (int)id;
    }

    /* ---- chrome ---- */

    /* Works out which slice of a list fits, keeping the cursor in view. Lists
     * here can be longer than their card, so nothing may be drawn past the
     * bottom edge. */
    struct ListView {
        int first;
        int visible;
    };

    ListView WindowFor(int cursor, int count, int top, int bottom, int rowH) {
        ListView v{ 0, std::max(1, (bottom - top) / rowH) };
        if (count > v.visible) {
            if (cursor >= v.visible) {
                v.first = cursor - v.visible + 1;
            }
            v.first = std::min(v.first, count - v.visible);
            v.first = std::max(v.first, 0);
        }
        return v;
    }

    void DrawScrollbar(int x, int top, int bottom, int first, int count, int visible) {
        if (count <= visible) {
            return;
        }
        const int trackH = bottom - top;
        const int thumbH = std::max(30, trackH * visible / count);
        const int thumbY = top + (trackH - thumbH) * first / std::max(1, count - visible);
        ui::RoundRect(x, top, 4, trackH, 2, ui::Border);
        ui::RoundRect(x, thumbY, 4, thumbH, 2, ui::Accent);
    }

    void DrawCard(int x, int y, int w, int h) {
        ui::RoundRect(x, y, w, h, 14, ui::Surface);
        ui::RoundRectOutline(x, y, w, h, 14, ui::Border);
    }

    constexpr int ChipW = 78;
    constexpr int ChipH = 60;
    constexpr int ChipGap = 6;

    /* Fixed-width chips so the header stays stable as values change, and so a
     * row of them cannot drift off the right edge. Text steps down a size
     * rather than spilling past the chip edge, so no value can clip. */
    void DrawChip(int x, int y, int w, const std::string &label, const std::string &value,
                  SDL_Color valueColor, bool highlight) {
        ui::RoundRect(x, y, w, ChipH, 12, highlight ? ui::AccentSoft : ui::SurfaceAlt);
        if (highlight) {
            ui::RoundRectOutline(x, y, w, ChipH, 12, ui::Accent);
        }

        const int inner = w - 12;

        ui::Font labelFont = ui::FontSmall;
        if (ui::TextWidth(label, labelFont) > inner) {
            labelFont = ui::FontTiny;
        }
        ui::TextCentered(label, x + w / 2, y + 10, labelFont, highlight ? ui::Accent : ui::TextDim);

        ui::Font valueFont = ui::FontBody;
        if (ui::TextWidth(value, valueFont) > inner) {
            valueFont = ui::FontSmall;
        }
        ui::TextCentered(value, x + w / 2, y + 30, valueFont, valueColor);
    }

    SDL_Color TempColor(float tempC) {
        if (tempC < 0.0f) {
            return ui::TextFaint;
        }
        if (tempC >= 60.0f) {
            return ui::Danger;
        }
        if (tempC >= 50.0f) {
            return ui::Warn;
        }
        return ui::Good;
    }

    std::string TempText(float tempC, FanSensor sensor) {
        if (tempC >= 0.0f) {
            return std::to_string(RoundToInt(tempC)) + "C";
        }
        /* Distinguish "this needs Horizon OC" from "this read failed". */
        return SensorNeedsHorizonOc(sensor) ? "N/A" : "--";
    }

    void DrawHeader() {
        ui::Rect(0, 0, ui::ScreenW, HeaderH, ui::Surface);
        ui::Line(0, HeaderH - 1, ui::ScreenW, HeaderH - 1, ui::Border);

        ui::Text_("NXFanControl", Margin, 18, ui::FontTitle, ui::Text);
        const int w = ui::TextWidth("NXFanControl", ui::FontTitle);
        ui::Text_("Manager", Margin + w + 10, 26, ui::FontHead, ui::Accent);

        /* Kept short: the chip row starts partway across and sits at the same
         * height. The N/A chips already show what a missing Horizon OC costs. */
        std::string sub = GetConsoleModelName();
        sub += g_app.sysmoduleUp ? "  ·  sysmodule on" : "  ·  sysmodule OFF";
        sub += g_app.horizonOc ? "  ·  Horizon OC" : "  ·  no Horizon OC";

        ui::Text_(sub, Margin, 62, ui::FontSmall, g_app.sysmoduleUp ? ui::TextDim : ui::Danger);

        /* One chip per temperature source, then fan and dock state. The chip
         * for the sensor driving the curve is highlighted. */
        constexpr int StateW = 118;
        const int totalW = (FanSensor_Count + 1) * (ChipW + ChipGap) + StateW;
        int x = ui::ScreenW - Margin - totalW;

        for (int i = 0; i < FanSensor_Count; ++i) {
            const FanSensor s = (FanSensor)i;
            const float t = g_app.sensorTemps[i];
            DrawChip(x, 16, ChipW, GetSensorName(s), TempText(t, s), TempColor(t),
                     (u32)i == g_app.fanSensor);
            x += ChipW + ChipGap;
        }

        const std::string fan = g_app.liveFanSpeed >= 0 ? std::to_string(RoundToInt(g_app.liveFanSpeed)) + "%" : "--";
        DrawChip(x, 16, ChipW, "FAN", fan, ui::Accent, false);
        x += ChipW + ChipGap;

        DrawChip(x, 16, StateW, "STATE", g_app.isDocked ? "Docked" : "Handheld", ui::Text, false);

    }

    void DrawTabs() {
        int x = Margin;
        for (int i = 0; i < Tab_Count; ++i) {
            const std::string name = TabNames[i];
            const int w = ui::TextWidth(name, ui::FontBody) + 44;
            const bool sel = (g_app.tab == i);

            if (sel) {
                ui::RoundRect(x, TabBarY + 8, w, 38, 10, ui::AccentSoft);
            }
            ui::TextCentered(name, x + w / 2, TabBarY + 16, ui::FontBody, sel ? ui::Accent : ui::TextDim);
            x += w + 8;
        }

        ui::TextRight("ZL / ZR  switch tab", ui::ScreenW - Margin, TabBarY + 18, ui::FontSmall, ui::TextFaint);
    }

    void DrawFooter(const std::string &hints) {
        ui::Line(0, FooterY, ui::ScreenW, FooterY, ui::Border);

        if (!g_app.status.empty() && armGetSystemTick() < g_app.statusUntilTick) {
            ui::Text_(g_app.status, Margin, FooterY + 20, ui::FontBody, ui::Accent);
        } else {
            ui::Text_(hints, Margin, FooterY + 22, ui::FontSmall, ui::TextDim);
        }

        ui::TextRight("+  Exit", ui::ScreenW - Margin, FooterY + 22, ui::FontSmall, ui::TextDim);
    }

    /* ---- profiles tab ---- */

    void DrawProfilesTab() {
        DrawCard(Margin, ContentY, ListW, ContentH);
        ui::Text_("PROFILES", Margin + 22, ContentY + 18, ui::FontSmall, ui::TextFaint);
        ui::TextRight(std::to_string(g_app.profileCount) + " / " + std::to_string(MaxProfiles),
                      Margin + ListW - 22, ContentY + 18, ui::FontSmall, ui::TextFaint);

        const int rowH = 62;
        const int listTop = ContentY + 48;
        const int listBottom = ContentY + ContentH - 10;
        const ListView view = WindowFor(g_app.profileCursor, (int)g_app.profileCount, listTop, listBottom, rowH);

        int y = listTop;

        for (u32 i = view.first; i < g_app.profileCount && (int)i < view.first + view.visible; ++i) {
            const u32 id = g_app.profileIds[i];
            const bool sel = ((int)i == g_app.profileCursor);
            const bool active = (id == g_app.activeProfile);

            if (sel) {
                ui::RoundRect(Margin + 12, y, ListW - 24, rowH - 8, 10, ui::SurfaceAlt);
                ui::Rect(Margin + 12, y + 10, 3, rowH - 28, ui::Accent);
            }

            ui::Text_(g_app.profileName(id), Margin + 30, y + 14, ui::FontBody, sel ? ui::Text : ui::TextDim);

            if (active) {
                const int bw = ui::TextWidth("ACTIVE", ui::FontSmall) + 20;
                ui::RoundRect(Margin + ListW - 24 - bw, y + 15, bw, 24, 8, ui::AccentSoft);
                ui::TextCentered("ACTIVE", Margin + ListW - 24 - bw / 2, y + 17, ui::FontSmall, ui::Accent);
            }

            y += rowH;
        }

        DrawScrollbar(Margin + ListW - 10, listTop, listBottom, view.first, (int)g_app.profileCount, view.visible);

        /* preview panel */
        DrawCard(PanelX, ContentY, PanelW, ContentH);

        std::string title = g_app.profileCount > 0 ? g_app.profileName(g_app.profileIds[g_app.profileCursor]) : "No profiles";
        ui::Text_(title, PanelX + 24, ContentY + 16, ui::FontHead, ui::Text);
        ui::Text_("Handheld curve preview", PanelX + 24, ContentY + 50, ui::FontSmall, ui::TextFaint);

        ui::CurveGraph(PanelX + 16, ContentY + 84, PanelW - 32, ContentH - 108,
                       g_preview.points, g_preview.count,
                       -1.0f, -1);
    }

    void DrawDeleteConfirm() {
        ui::Rect(0, 0, ui::ScreenW, ui::ScreenH, SDL_Color{ 0, 0, 0, 170 });

        const int w = 560, h = 220;
        const int x = (ui::ScreenW - w) / 2, y = (ui::ScreenH - h) / 2;
        ui::RoundRect(x, y, w, h, 16, ui::Surface);
        ui::RoundRectOutline(x, y, w, h, 16, ui::Danger);

        ui::TextCentered("Delete profile?", x + w / 2, y + 34, ui::FontHead, ui::Text);
        const std::string name = g_app.profileName(g_app.profileIds[g_app.profileCursor]);
        ui::TextCentered(name, x + w / 2, y + 84, ui::FontTitle, ui::Danger);
        ui::TextCentered("Its fan curves will be removed.", x + w / 2, y + 132, ui::FontSmall, ui::TextDim);
        ui::TextCentered("A  Delete        B  Cancel", x + w / 2, y + 172, ui::FontBody, ui::TextDim);
    }

    void DrawPresetPicker() {
        ui::Rect(0, 0, ui::ScreenW, ui::ScreenH, SDL_Color{ 0, 0, 0, 170 });

        const int w = 680, h = 340;
        const int x = (ui::ScreenW - w) / 2, y = (ui::ScreenH - h) / 2;
        ui::RoundRect(x, y, w, h, 16, ui::Surface);
        ui::RoundRectOutline(x, y, w, h, 16, ui::Accent);

        ui::Text_("Start \"" + std::string(g_pendingName) + "\" from", x + 32, y + 26, ui::FontHead, ui::Text);

        int ry = y + 78;
        for (int i = 0; i < PresetChoiceCount; ++i) {
            const bool sel = (g_presetCursor == i);
            if (sel) {
                ui::RoundRect(x + 20, ry, w - 40, 62, 10, ui::SurfaceAlt);
                ui::Rect(x + 20, ry + 10, 3, 42, ui::Accent);
            }
            ui::Text_(PresetChoices[i].name, x + 40, ry + 8, ui::FontBody, sel ? ui::Text : ui::TextDim);
            ui::Text_(PresetChoices[i].help, x + 40, ry + 34, ui::FontSmall, ui::TextFaint);
            ry += 68;
        }

        ui::TextCentered("A  Create        B  Cancel", x + w / 2, y + h - 40, ui::FontBody, ui::TextDim);
    }

    void HandlePresetPicker(u64 kDown, u64 kHeld) {
        if (g_repUp.fire(kDown, kHeld, HidNpadButton_Up)) {
            g_presetCursor = (g_presetCursor + PresetChoiceCount - 1) % PresetChoiceCount;
        }
        if (g_repDown.fire(kDown, kHeld, HidNpadButton_Down)) {
            g_presetCursor = (g_presetCursor + 1) % PresetChoiceCount;
        }

        if (kDown & HidNpadButton_B) {
            g_modal = Modal_None;
            return;
        }

        if (!(kDown & HidNpadButton_A)) {
            return;
        }

        const PresetChoice &choice = PresetChoices[g_presetCursor];
        u32 newId = 0;
        bool ok = false;

        if (choice.preset < 0) {
            const u32 seed = g_app.profileCount > 0 ? g_app.profileIds[g_app.profileCursor] : 0;
            ok = CreateProfile(g_pendingName, seed, &newId);
        } else {
            ok = CreateProfileFromPreset(g_pendingName, (CurvePreset)choice.preset, &newId);
        }

        if (ok) {
            g_app.reloadProfiles();
            for (u32 i = 0; i < g_app.profileCount; ++i) {
                if (g_app.profileIds[i] == newId) {
                    g_app.profileCursor = (int)i;
                    break;
                }
            }
            g_previewFor = -1;
            RefreshPreview();
            g_app.setStatus(std::string("Created from ") + choice.name);
        } else {
            g_app.setStatus("Could not create profile");
        }

        g_modal = Modal_None;
    }

    /* ---- games tab ---- */

    void DrawGamesTab() {
        DrawCard(Margin, ContentY, ListW, ContentH);
        ui::Text_("ASSIGNED GAMES", Margin + 22, ContentY + 18, ui::FontSmall, ui::TextFaint);
        ui::TextRight(std::to_string(g_app.mappingCount), Margin + ListW - 22, ContentY + 18, ui::FontSmall, ui::TextFaint);

        if (g_app.mappingCount == 0) {
            ui::Text_("No games assigned yet.", Margin + 26, ContentY + 60, ui::FontBody, ui::TextDim);
        }

        const int rowH = 58;
        const int listTop = ContentY + 48;
        const int listBottom = ContentY + ContentH - 10;
        const ListView view = WindowFor(g_app.gameCursor, (int)g_app.mappingCount, listTop, listBottom, rowH);

        int y = listTop;

        for (u32 i = view.first; i < g_app.mappingCount && (int)i < view.first + view.visible; ++i) {
            const bool sel = ((int)i == g_app.gameCursor);
            if (sel) {
                ui::RoundRect(Margin + 12, y, ListW - 24, rowH - 8, 10, ui::SurfaceAlt);
                ui::Rect(Margin + 12, y + 10, 3, rowH - 28, ui::Accent);
            }

            char titleText[24];
            snprintf(titleText, sizeof(titleText), "%016lX", g_app.mappings[i].titleId);

            ui::Text_(titleText, Margin + 30, y + 6, ui::FontSmall, sel ? ui::Text : ui::TextDim);
            ui::Text_(g_app.profileName(g_app.mappings[i].profileId), Margin + 30, y + 26, ui::FontBody,
                      sel ? ui::Accent : ui::TextFaint);
            y += rowH;
        }

        DrawScrollbar(Margin + ListW - 10, listTop, listBottom, view.first, (int)g_app.mappingCount, view.visible);

        /* explanation panel */
        DrawCard(PanelX, ContentY, PanelW, ContentH);
        ui::Text_("Per-Game Profiles", PanelX + 24, ContentY + 18, ui::FontHead, ui::Text);

        const bool on = g_app.gameProfiles;
        const int tw = 62, th = 30;
        const int tx = PanelX + PanelW - 32 - tw;
        ui::RoundRect(tx, ContentY + 20, tw, th, th / 2, on ? ui::Accent : ui::Border);
        ui::Circle(on ? tx + tw - th / 2 : tx + th / 2, ContentY + 20 + th / 2, th / 2 - 4, ui::Surface);

        int ty = ContentY + 74;
        const char *lines[] = {
            "Bind a game to a profile and the sysmodule switches",
            "to it automatically when that game is running.",
            "",
            "Assignments are made from the Tesla overlay, because",
            "it is the only part of the tool that runs while a game",
            "is in the foreground:",
            "",
            "   1.  Launch the game",
            "   2.  Open the overlay  ->  Profile",
            "   3.  Pick the profile you want, then Assign",
            "",
            "Games with no assignment use the selected profile.",
        };
        for (const char *line : lines) {
            ui::Text_(line, PanelX + 24, ty, ui::FontBody, ui::TextDim);
            ty += 30;
        }

        ui::Text_("A  toggle per-game profiles        X  unassign selected",
                  PanelX + 24, ContentY + ContentH - 40, ui::FontSmall, ui::TextFaint);
    }

    void DrawUnbindConfirm() {
        ui::Rect(0, 0, ui::ScreenW, ui::ScreenH, SDL_Color{ 0, 0, 0, 170 });

        const int w = 560, h = 210;
        const int x = (ui::ScreenW - w) / 2, y = (ui::ScreenH - h) / 2;
        ui::RoundRect(x, y, w, h, 16, ui::Surface);
        ui::RoundRectOutline(x, y, w, h, 16, ui::Danger);

        ui::TextCentered("Remove assignment?", x + w / 2, y + 34, ui::FontHead, ui::Text);

        char titleText[24];
        snprintf(titleText, sizeof(titleText), "%016lX", g_app.mappings[g_app.gameCursor].titleId);
        ui::TextCentered(titleText, x + w / 2, y + 86, ui::FontBody, ui::Danger);
        ui::TextCentered("The game falls back to the selected profile.", x + w / 2, y + 120, ui::FontSmall, ui::TextDim);
        ui::TextCentered("A  Remove        B  Cancel", x + w / 2, y + 158, ui::FontBody, ui::TextDim);
    }

    void HandleGamesInput(u64 kDown, u64 kHeld) {
        if (g_modal == Modal_UnbindGame) {
            if (kDown & HidNpadButton_A) {
                if (ClearProfileForTitle(g_app.mappings[g_app.gameCursor].titleId)) {
                    g_app.setStatus("Assignment removed");
                    g_app.reloadMappings();
                }
                g_modal = Modal_None;
            } else if (kDown & HidNpadButton_B) {
                g_modal = Modal_None;
            }
            return;
        }

        if (g_app.mappingCount > 0) {
            if (g_repUp.fire(kDown, kHeld, HidNpadButton_Up)) {
                g_app.gameCursor = (g_app.gameCursor + g_app.mappingCount - 1) % g_app.mappingCount;
            }
            if (g_repDown.fire(kDown, kHeld, HidNpadButton_Down)) {
                g_app.gameCursor = (g_app.gameCursor + 1) % g_app.mappingCount;
            }
        }

        if (kDown & HidNpadButton_A) {
            g_app.gameProfiles = !g_app.gameProfiles;
            SetGameProfilesEnabled(g_app.gameProfiles);
        }

        if ((kDown & HidNpadButton_X) && g_app.mappingCount > 0) {
            g_modal = Modal_UnbindGame;
        }
    }

    void HandleProfilesInput(u64 kDown, u64 kHeld) {
        if (g_modal == Modal_DeleteProfile) {
            if (kDown & HidNpadButton_A) {
                const u32 id = g_app.profileIds[g_app.profileCursor];
                if (DeleteProfile(id)) {
                    g_app.setStatus("Profile deleted");
                    g_app.reloadProfiles();
                    g_previewFor = -1;
                    RefreshPreview();
                    g_app.reloadCurves();
                } else {
                    g_app.setStatus("Can't delete the last profile");
                }
                g_modal = Modal_None;
            } else if (kDown & (HidNpadButton_B | HidNpadButton_Minus)) {
                g_modal = Modal_None;
            }
            return;
        }

        if (g_app.profileCount > 0) {
            if (g_repUp.fire(kDown, kHeld, HidNpadButton_Up)) {
                g_app.profileCursor = (g_app.profileCursor + g_app.profileCount - 1) % g_app.profileCount;
                RefreshPreview();
            }
            if (g_repDown.fire(kDown, kHeld, HidNpadButton_Down)) {
                g_app.profileCursor = (g_app.profileCursor + 1) % g_app.profileCount;
                RefreshPreview();
            }
        }

        if ((kDown & HidNpadButton_A) && g_app.profileCount > 0) {
            const u32 id = g_app.profileIds[g_app.profileCursor];
            if (id == g_app.activeProfile) {
                g_app.setStatus("Already active");
            } else if (SetActiveProfileId(id)) {
                g_app.reloadProfiles();
                g_app.reloadCurves();
                g_app.setStatus("Switched to " + g_app.profileName(id));
            }
        }

        if ((kDown & HidNpadButton_X) && g_app.profileCount > 0) {
            const u32 id = g_app.profileIds[g_app.profileCursor];
            char current[MaxProfileNameLength + 1];
            GetProfileName(id, current, sizeof(current));

            char entered[MaxProfileNameLength + 1];
            if (PromptText("Profile name", current, entered, sizeof(entered))) {
                if (SetProfileName(id, entered)) {
                    g_app.setStatus("Renamed");
                } else {
                    g_app.setStatus("That name can't be used");
                }
            }
        }

        if (kDown & HidNpadButton_Y) {
            if (g_app.profileCount >= MaxProfiles) {
                g_app.setStatus("Profile limit reached");
            } else {
                if (PromptText("New profile name", "", g_pendingName, sizeof(g_pendingName))) {
                    g_presetCursor = 0;
                    g_modal = Modal_PresetPicker;
                }
            }
        }

        if (g_app.profileCount > 1) {
            const int delta = (kDown & HidNpadButton_L) ? -1 : ((kDown & HidNpadButton_R) ? 1 : 0);
            if (delta != 0) {
                const u32 id = g_app.profileIds[g_app.profileCursor];
                if (MoveProfile(id, delta)) {
                    g_app.reloadProfiles();
                    for (u32 i = 0; i < g_app.profileCount; ++i) {
                        if (g_app.profileIds[i] == id) {
                            g_app.profileCursor = (int)i;
                            break;
                        }
                    }
                }
            }
        }

        if ((kDown & HidNpadButton_Minus) && g_app.profileCount > 0) {
            if (g_app.profileCount <= 1) {
                g_app.setStatus("Can't delete the last profile");
            } else {
                g_modal = Modal_DeleteProfile;
            }
        }
    }

    /* ---- curve tab ---- */

    void DrawCurveTab() {
        CurveBuffer &curve = g_app.currentCurve();

        DrawCard(Margin, ContentY, ListW, ContentH);

        ui::Text_("EDITING", Margin + 22, ContentY + 18, ui::FontSmall, ui::TextFaint);
        ui::Text_(g_app.activeProfileName(), Margin + 22, ContentY + 40, ui::FontHead, ui::Text);

        /* handheld / docked selector */
        if (g_app.dockedOverride) {
            const int segY = ContentY + 84;
            const int segW = (ListW - 44) / 2;
            for (int i = 0; i < 2; ++i) {
                const bool sel = (g_app.editingDocked == (i == 1));
                const int sx = Margin + 22 + i * segW;
                if (sel) {
                    ui::RoundRect(sx, segY, segW, 36, 9, ui::AccentSoft);
                }
                ui::TextCentered(i == 0 ? "Handheld" : "Docked", sx + segW / 2, segY + 6, ui::FontBody,
                                 sel ? ui::Accent : ui::TextDim);
            }
            ui::Text_("Y  switch curve", Margin + 22, ContentY + 128, ui::FontSmall, ui::TextFaint);
        } else {
            ui::Text_("Single curve", Margin + 22, ContentY + 92, ui::FontSmall, ui::TextFaint);
            ui::Text_("Enable Docked Profiles in", Margin + 22, ContentY + 116, ui::FontSmall, ui::TextFaint);
            ui::Text_("Settings for a separate docked curve.", Margin + 22, ContentY + 136, ui::FontSmall, ui::TextFaint);
        }

        /* point list */
        const int listTop = ContentY + 170;
        ui::Text_("POINTS", Margin + 22, listTop - 26, ui::FontSmall, ui::TextFaint);
        ui::TextRight(std::to_string(curve.count) + " / " + std::to_string(MAX_TABLE_ENTRIES),
                      Margin + ListW - 22, listTop - 26, ui::FontSmall, ui::TextFaint);

        const int rowH = 40;
        const int listBottom = ContentY + ContentH - 10;
        const ListView view = WindowFor(g_app.pointCursor, (int)curve.count, listTop, listBottom, rowH);

        for (u32 i = view.first; i < curve.count && (int)i < view.first + view.visible; ++i) {
            const bool sel = ((int)i == g_app.pointCursor);
            const int ry = listTop + ((int)i - view.first) * rowH;

            if (sel) {
                ui::RoundRect(Margin + 12, ry, ListW - 24, rowH - 6, 8, ui::SurfaceAlt);
            }
            ui::Text_(std::to_string(curve.points[i].temperature_c) + " C",
                      Margin + 30, ry + 6, ui::FontBody, sel ? ui::Text : ui::TextDim);
            ui::TextRight(std::to_string(LevelToPercent(curve.points[i].fanLevel_f)) + " %",
                          Margin + ListW - 30, ry + 6, ui::FontBody, sel ? ui::Accent : ui::TextDim);
        }

        /* graph */
        DrawCard(PanelX, ContentY, PanelW, ContentH);
        ui::Text_(g_app.editingDocked ? "Docked curve" : "Handheld curve",
                  PanelX + 24, ContentY + 16, ui::FontHead, ui::Text);

        if (g_app.liveTemp >= 0) {
            ui::TextRight("live " + std::to_string(RoundToInt(g_app.liveTemp)) + "C",
                          PanelX + PanelW - 24, ContentY + 24, ui::FontSmall, ui::Warn);
        }

        /* Only show the live marker on the curve that is actually in use. */
        const bool curveInUse = (!g_app.dockedOverride) || (g_app.editingDocked == g_app.isDocked);

        ui::CurveGraph(PanelX + 16, ContentY + 62, PanelW - 32, ContentH - 86,
                       curve.points, curve.count,
                       curveInUse ? g_app.liveTemp : -1.0f,
                       g_app.pointCursor);
    }

    void HandleCurveInput(u64 kDown, u64 kHeld) {
        CurveBuffer &curve = g_app.currentCurve();

        if (curve.count > 0) {
            if (g_repUp.fire(kDown, kHeld, HidNpadButton_Up)) {
                g_app.pointCursor = (g_app.pointCursor + curve.count - 1) % curve.count;
            }
            if (g_repDown.fire(kDown, kHeld, HidNpadButton_Down)) {
                g_app.pointCursor = (g_app.pointCursor + 1) % curve.count;
            }
        }

        const u32 idx = (u32)g_app.pointCursor;

        /* left/right adjust fan level */
        if (g_repLeft.fire(kDown, kHeld, HidNpadButton_Left) && idx < curve.count) {
            int pct = LevelToPercent(curve.points[idx].fanLevel_f) - 5;
            curve.setLevel(idx, std::max(0, pct) / 100.0f);
            curve.save();
        }
        if (g_repRight.fire(kDown, kHeld, HidNpadButton_Right) && idx < curve.count) {
            int pct = LevelToPercent(curve.points[idx].fanLevel_f) + 5;
            curve.setLevel(idx, std::min(100, pct) / 100.0f);
            curve.save();
        }

        /* L/R adjust temperature */
        if (g_repL.fire(kDown, kHeld, HidNpadButton_L) && idx < curve.count) {
            const int want = curve.points[idx].temperature_c - 5;
            if (curve.setTemp(idx, want)) {
                const int temp = want;
                curve.save();
                for (u32 i = 0; i < curve.count; ++i) {
                    if (curve.points[i].temperature_c == temp) {
                        g_app.pointCursor = (int)i;
                        break;
                    }
                }
            }
        }
        if (g_repR.fire(kDown, kHeld, HidNpadButton_R) && idx < curve.count) {
            const int want = curve.points[idx].temperature_c + 5;
            if (curve.setTemp(idx, want)) {
                const int temp = want;
                curve.save();
                for (u32 i = 0; i < curve.count; ++i) {
                    if (curve.points[i].temperature_c == temp) {
                        g_app.pointCursor = (int)i;
                        break;
                    }
                }
            }
        }

        if (kDown & HidNpadButton_A) {
            if (curve.addPoint()) {
                g_app.setStatus("Point added");
            } else {
                g_app.setStatus("No free temperature slot");
            }
        }

        if (kDown & HidNpadButton_X) {
            if (curve.removePoint(idx)) {
                if (g_app.pointCursor >= (int)curve.count) {
                    g_app.pointCursor = (int)curve.count - 1;
                }
                g_app.setStatus("Point removed");
            } else {
                g_app.setStatus("A curve needs at least two points");
            }
        }

        if ((kDown & HidNpadButton_Y) && g_app.dockedOverride) {
            g_app.editingDocked = !g_app.editingDocked;
            g_app.pointCursor = 0;
        }
    }

    /* ---- settings tab ---- */

    enum SettingRow {
        Set_Enabled = 0,
        Set_DockedProfiles,
        Set_Sensor,
        Set_FastTemp,
        Set_SlowInterval,
        Set_FastInterval,
        Set_ConfigInterval,
        Set_EnableInterval,
        Set_DockedInterval,
        Set_Count,
    };

    enum SettingKind {
        Kind_Toggle = 0,
        Kind_Number,
        Kind_Sensor,
    };

    struct SettingDef {
        const char *name;
        const char *help;
        const char *unit;
        u32 *value;
        u32 min, max, step;
        SettingKind kind;
        bool *flag;
    };

    void BuildSettings(SettingDef *out) {
        out[Set_Enabled]        = { "Fan control", "Master switch for the sysmodule's custom curve", "", nullptr, 0, 0, 0, Kind_Toggle, &g_app.enabled };
        out[Set_DockedProfiles] = { "Docked profiles", "Give each profile a separate docked curve", "", nullptr, 0, 0, 0, Kind_Toggle, &g_app.dockedOverride };
        out[Set_Sensor]         = { "Curve sensor", GetSensorDescription((FanSensor)g_app.fanSensor), "", &g_app.fanSensor, 0, FanSensor_Count - 1, 1, Kind_Sensor, nullptr };
        out[Set_FastTemp]       = { "High refresh temp", "Above this, poll at the faster interval", "C", &g_app.fastRefreshTemperatureC, MinFastRefreshTempC, MaxFastRefreshTempC, 5, Kind_Number, nullptr };
        out[Set_SlowInterval]   = { "Low temp interval", "Poll rate while cool", "ms", &g_app.slowRefreshIntervalMs, MinPollIntervalMs, MaxPollIntervalMs, 5, Kind_Number, nullptr };
        out[Set_FastInterval]   = { "High temp interval", "Poll rate while hot", "ms", &g_app.fastRefreshIntervalMs, MinPollIntervalMs, MaxPollIntervalMs, 5, Kind_Number, nullptr };
        out[Set_ConfigInterval] = { "Config reload", "How often the sysmodule re-reads this config", "ms", &g_app.configRefreshIntervalMs, MinCheckIntervalMs, MaxCheckIntervalMs, 100, Kind_Number, nullptr };
        out[Set_EnableInterval] = { "Enable check", "How often it checks the master switch", "ms", &g_app.enableRefreshIntervalMs, MinCheckIntervalMs, MaxCheckIntervalMs, 100, Kind_Number, nullptr };
        out[Set_DockedInterval] = { "Dock check", "How often it checks dock state", "ms", &g_app.dockedRefreshIntervalMs, MinCheckIntervalMs, MaxCheckIntervalMs, 100, Kind_Number, nullptr };
    }

    void DrawSettingsTab() {
        SettingDef defs[Set_Count];
        BuildSettings(defs);

        const int cardW = ui::ScreenW - Margin * 2;
        DrawCard(Margin, ContentY, cardW, ContentH);

        const int rowH = 62;
        const int listTop = ContentY + 14;
        const int listBottom = ContentY + ContentH - 10;
        const ListView view = WindowFor(g_app.settingsCursor, Set_Count, listTop, listBottom, rowH);

        int y = listTop;

        for (int i = view.first; i < Set_Count && i < view.first + view.visible; ++i) {
            const bool sel = (g_app.settingsCursor == i);
            if (sel) {
                ui::RoundRect(Margin + 12, y, cardW - 24, rowH - 6, 10, ui::SurfaceAlt);
                ui::Rect(Margin + 12, y + 10, 3, rowH - 26, ui::Accent);
            }

            ui::Text_(defs[i].name, Margin + 32, y + 8, ui::FontBody, sel ? ui::Text : ui::TextDim);
            ui::Text_(defs[i].help, Margin + 32, y + 34, ui::FontSmall, ui::TextFaint);

            if (defs[i].kind == Kind_Toggle) {
                const bool on = *defs[i].flag;
                const int tw = 62, th = 30;
                const int tx = Margin + cardW - 32 - tw;
                ui::RoundRect(tx, y + 14, tw, th, th / 2, on ? ui::Accent : ui::Border);
                ui::Circle(on ? tx + tw - th / 2 : tx + th / 2, y + 14 + th / 2, th / 2 - 4, ui::Surface);
            } else if (defs[i].kind == Kind_Sensor) {
                const FanSensor s = (FanSensor)g_app.fanSensor;
                const bool usable = !SensorNeedsHorizonOc(s) || g_app.horizonOc;
                ui::TextRight(GetSensorName(s), Margin + cardW - 32, y + 16, ui::FontHead,
                              usable ? (sel ? ui::Accent : ui::TextDim) : ui::Danger);
            } else {
                const std::string value = std::to_string(*defs[i].value) + " " + defs[i].unit;
                ui::TextRight(value, Margin + cardW - 32, y + 16, ui::FontHead, sel ? ui::Accent : ui::TextDim);
            }

            y += rowH;
        }

        DrawScrollbar(Margin + cardW - 10, listTop, listBottom, view.first, Set_Count, view.visible);
    }

    void HandleSettingsInput(u64 kDown, u64 kHeld) {
        SettingDef defs[Set_Count];
        BuildSettings(defs);

        if (g_repUp.fire(kDown, kHeld, HidNpadButton_Up)) {
            g_app.settingsCursor = (g_app.settingsCursor + Set_Count - 1) % Set_Count;
        }
        if (g_repDown.fire(kDown, kHeld, HidNpadButton_Down)) {
            g_app.settingsCursor = (g_app.settingsCursor + 1) % Set_Count;
        }

        SettingDef &def = defs[g_app.settingsCursor];

        if (def.kind == Kind_Sensor) {
            int delta = 0;
            if (g_repLeft.fire(kDown, kHeld, HidNpadButton_Left)) {
                delta = -1;
            } else if (g_repRight.fire(kDown, kHeld, HidNpadButton_Right) || (kDown & HidNpadButton_A)) {
                delta = 1;
            }
            if (delta != 0) {
                g_app.fanSensor = (g_app.fanSensor + FanSensor_Count + delta) % FanSensor_Count;
                SetFanSensor(g_app.fanSensor);

                const FanSensor s = (FanSensor)g_app.fanSensor;
                if (SensorNeedsHorizonOc(s) && !g_app.horizonOc) {
                    /* Allowed, but say plainly that it will not take effect
                     * until Horizon OC is present. */
                    g_app.setStatus(std::string(GetSensorName(s)) + " needs Horizon OC - the curve will use SoC until then");
                } else {
                    g_app.setStatus(std::string("Curve now follows the ") + GetSensorName(s) + " sensor");
                }
            }
            return;
        }

        if (def.kind == Kind_Toggle) {
            if (kDown & (HidNpadButton_A | HidNpadButton_Left | HidNpadButton_Right)) {
                *def.flag = !*def.flag;

                /* These two are written immediately: they are single flags and
                 * the user expects the fan to react at once. */
                if (def.flag == &g_app.enabled) {
                    SetEnabled(ConfigSection, g_app.enabled);
                } else {
                    SetDockedOverride(ConfigSection, g_app.dockedOverride);
                    if (!g_app.dockedOverride) {
                        g_app.editingDocked = false;
                    }
                    g_app.reloadCurves();
                }
            }
            return;
        }

        bool changed = false;
        if (g_repLeft.fire(kDown, kHeld, HidNpadButton_Left)) {
            *def.value = (*def.value > def.min + def.step) ? (*def.value - def.step) : def.min;
            changed = true;
        }
        if (g_repRight.fire(kDown, kHeld, HidNpadButton_Right)) {
            *def.value = std::min(*def.value + def.step, def.max);
            changed = true;
        }
        if (changed) {
            MarkSettingsDirty();
        }
    }

    /* ---- frame ---- */

    void UpdateLiveReadings() {
        static u64 last = 0;
        const u64 now = armGetSystemTick();
        /* now < last would mean the counter went backwards (a reset across
         * sleep); refresh immediately rather than subtracting into a huge
         * value. */
        if (last != 0 && now >= last && armTicksToNs(now - last) < 500'000'000ULL) {
            return;
        }
        last = now;

        for (int i = 0; i < FanSensor_Count; ++i) {
            g_app.sensorTemps[i] = ReadSensorOrNegative((FanSensor)i);
        }
        g_app.liveTemp     = g_app.sensorTemps[g_app.fanSensor];
        g_app.liveFanSpeed = GetFanSpeed();
        g_app.isDocked     = IsDocked();
        g_app.sysmoduleUp  = IsRunning() != 0;
        g_app.horizonOc    = IsHorizonOcAvailable();
    }

    const char *HintsForTab() {
        switch (g_app.tab) {
            case Tab_Profiles: return "A  Activate     X  Rename     Y  New     L/R  Reorder     -  Delete";
            case Tab_Curve:    return "D-Pad  Select / Level     L/R  Temperature     A  Add     X  Remove     Y  Curve";
            case Tab_Games:    return "A  Toggle per-game profiles     X  Unassign selected";
            default:           return "D-Pad  Select     Left/Right  Adjust     A  Toggle";
        }
    }

}

int main(int argc, char **argv) {
    /* Config paths are relative to the SD root; hbmenu leaves the working
     * directory wherever the NRO was launched from. */
    chdir("sdmc:/");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("NXFanControl Manager", 0, 0, ui::ScreenW, ui::ScreenH, 0);
    if (window == nullptr) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (!ui::Init(renderer)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    pmdmntInitialize();
    InitializeSensors();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    g_app.reloadProfiles();
    g_app.reloadSettings();
    g_app.reloadMappings();
    g_app.reloadCurves();
    RefreshPreview();

    while (appletMainLoop()) {
        SDL_PumpEvents();

        padUpdate(&pad);
        const u64 kDown = padGetButtonsDown(&pad);
        const u64 kHeld = padGetButtons(&pad);

        if (kDown & HidNpadButton_Plus) {
            break;
        }

        if (g_modal == Modal_None && (kDown & (HidNpadButton_ZL | HidNpadButton_ZR))) {
            FlushSettingsIfDue(true);
            const int dir = (kDown & HidNpadButton_ZL) ? -1 : 1;
            g_app.tab = (Tab)((g_app.tab + Tab_Count + dir) % Tab_Count);

            /* Pick up anything changed on another tab. */
            if (g_app.tab == Tab_Curve) {
                g_app.reloadCurves();
            } else if (g_app.tab == Tab_Games) {
                g_app.reloadMappings();
            }
        } else if (g_modal == Modal_PresetPicker) {
            HandlePresetPicker(kDown, kHeld);
        } else {
            switch (g_app.tab) {
                case Tab_Profiles: HandleProfilesInput(kDown, kHeld); break;
                case Tab_Curve:    HandleCurveInput(kDown, kHeld);    break;
                case Tab_Games:    HandleGamesInput(kDown, kHeld);    break;
                default:           HandleSettingsInput(kDown, kHeld); break;
            }
        }

        FlushSettingsIfDue(false);
        UpdateLiveReadings();

        ui::Clear(ui::Bg);
        DrawHeader();
        DrawTabs();

        switch (g_app.tab) {
            case Tab_Profiles: DrawProfilesTab(); break;
            case Tab_Curve:    DrawCurveTab();    break;
            case Tab_Games:    DrawGamesTab();    break;
            default:           DrawSettingsTab(); break;
        }

        DrawFooter(HintsForTab());

        switch (g_modal) {
            case Modal_DeleteProfile: DrawDeleteConfirm(); break;
            case Modal_PresetPicker:  DrawPresetPicker();  break;
            case Modal_UnbindGame:    DrawUnbindConfirm(); break;
            default: break;
        }

        ui::Present();
    }

    FlushSettingsIfDue(true);

    CloseSensors();
    pmdmntExit();
    ui::Exit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
