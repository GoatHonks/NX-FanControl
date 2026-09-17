#pragma once

#include <fancontrol.hpp>

#include <string>
#include <vector>

/* Shared application state for the manager. The manager is the full editor:
 * anything the overlay can change, it can change too, plus naming and
 * ordering which the overlay cannot offer without a keyboard. */

enum Tab {
    Tab_Profiles = 0,
    Tab_Curve,
    Tab_Games,
    Tab_Settings,
    Tab_Count,
};

struct CurveBuffer {
    TemperaturePoint points[MAX_TABLE_ENTRIES];
    u32  count   = 0;
    bool docked  = false;
    char section[ProfileSectionSize] = {};

    void bind(u32 profileId, bool isDocked);
    void load();
    bool save();
    bool addPoint();
    bool removePoint(u32 index);
    bool setTemp(u32 index, int temperatureC);
    void setLevel(u32 index, float level);
    bool tempTaken(int temperatureC, u32 exceptIndex) const;
};

struct AppState {
    Tab tab = Tab_Profiles;

    /* profiles */
    u32 profileIds[MaxProfiles] = {};
    u32 profileCount = 0;
    u32 activeProfile = 0;
    int profileCursor = 0;

    /* curve editing */
    CurveBuffer handheld;
    CurveBuffer docked;
    bool editingDocked = false;
    int  pointCursor   = 0;

    /* per-game bindings */
    TitleMapping mappings[MaxTitleMappings] = {};
    u32 mappingCount = 0;
    int gameCursor = 0;
    bool gameProfiles = false;

    /* settings */
    int  settingsCursor = 0;
    bool enabled        = false;
    bool dockedOverride = false;
    u32  fastRefreshTemperatureC = 0;
    u32  slowRefreshIntervalMs   = 0;
    u32  fastRefreshIntervalMs   = 0;
    u32  configRefreshIntervalMs = 0;
    u32  enableRefreshIntervalMs = 0;
    u32  dockedRefreshIntervalMs = 0;

    /* which sensor drives the curve */
    u32 fanSensor = FanSensor_Soc;

    /* live readings */
    /* -1 means unavailable; zero would read as a real 0C. */
    float sensorTemps[FanSensor_Count] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float liveTemp     = -1.0f;   /* the selected sensor, for the graph */
    float liveFanSpeed = -1.0f;
    bool  isDocked     = false;
    bool  sysmoduleUp  = false;
    bool  horizonOc    = false;

    /* transient status line */
    std::string status;
    u64 statusUntilTick = 0;

    void reloadProfiles();
    void reloadMappings();
    void reloadCurves();
    void reloadSettings();
    void saveSettings();
    void setStatus(const std::string &text);

    CurveBuffer &currentCurve() { return this->editingDocked ? this->docked : this->handheld; }
    std::string profileName(u32 id) const;
    std::string activeProfileName() const { return this->profileName(this->activeProfile); }
};

extern AppState g_app;

/* Opens the system keyboard. Returns false when cancelled. */
bool PromptText(const char *guide, const char *initial, char *out, size_t outSize);
