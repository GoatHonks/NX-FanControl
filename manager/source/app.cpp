#include "app.hpp"
#include "sensors.hpp"

#include <algorithm>

AppState g_app;

namespace {
    constexpr int MinTemp  = 20;
    constexpr int MaxTemp  = 80;
    constexpr int TempStep = 5;

    /* Same starting curve the overlay uses, so a profile created in either
     * place looks the same. */
    const TemperaturePoint DefaultCurve[] = {
        { .temperature_c = 25, .fanLevel_f = 0.00f },
        { .temperature_c = 30, .fanLevel_f = 0.00f },
        { .temperature_c = 35, .fanLevel_f = 0.00f },
        { .temperature_c = 40, .fanLevel_f = 0.00f },
        { .temperature_c = 45, .fanLevel_f = 0.00f },
        { .temperature_c = 50, .fanLevel_f = 0.30f },
        { .temperature_c = 55, .fanLevel_f = 0.40f },
        { .temperature_c = 60, .fanLevel_f = 0.60f },
        { .temperature_c = 65, .fanLevel_f = 0.70f },
        { .temperature_c = 70, .fanLevel_f = 1.00f },
    };
    constexpr u32 DefaultCurveCount = sizeof(DefaultCurve) / sizeof(DefaultCurve[0]);
}

/* ---- CurveBuffer ---- */

void CurveBuffer::bind(u32 profileId, bool isDocked) {
    this->docked    = isDocked;
    this->profileId = profileId;
    GetProfileSection(profileId, isDocked, this->section, sizeof(this->section));
}

void CurveBuffer::load() {
    this->count = LoadCurve(this->section, this->points, MAX_TABLE_ENTRIES);
    if (this->count == 0) {
        /* An empty docked curve starts as a copy of the same profile's handheld
         * curve - what the sysmodule falls back to anyway - rather than an
         * unrelated built-in curve. */
        if (this->docked) {
            char handheld[ProfileSectionSize];
            GetProfileSection(this->profileId, false, handheld, sizeof(handheld));
            this->count = LoadCurve(handheld, this->points, MAX_TABLE_ENTRIES);
        }
        if (this->count == 0) {
            memcpy(this->points, DefaultCurve, sizeof(DefaultCurve));
            this->count = DefaultCurveCount;
        }
        SaveCurve(this->section, this->points, this->count);
    }
    SortFanCurveTable(this->points, this->count);
}

bool CurveBuffer::save() {
    SortFanCurveTable(this->points, this->count);
    return SaveCurve(this->section, this->points, this->count);
}

bool CurveBuffer::tempTaken(int temperatureC, u32 exceptIndex) const {
    for (u32 i = 0; i < this->count; ++i) {
        if (i != exceptIndex && this->points[i].temperature_c == temperatureC) {
            return true;
        }
    }
    return false;
}

bool CurveBuffer::addPoint() {
    if (this->count >= MAX_TABLE_ENTRIES) {
        return false;
    }

    int freeTemp = -1;
    for (int t = MinTemp; t <= MaxTemp; t += TempStep) {
        if (!this->tempTaken(t, this->count)) {
            freeTemp = t;
            break;
        }
    }
    if (freeTemp < 0) {
        return false;
    }

    this->points[this->count].temperature_c = freeTemp;
    this->points[this->count].fanLevel_f    = std::clamp(InterpolateFanLevel(this->points, this->count, (float)freeTemp), 0.0f, 1.0f);
    this->count++;

    return this->save();
}

bool CurveBuffer::removePoint(u32 index) {
    if (this->count <= 2 || index >= this->count) {
        return false;
    }
    for (u32 i = index; i + 1 < this->count; ++i) {
        this->points[i] = this->points[i + 1];
    }
    this->count--;
    return this->save();
}

bool CurveBuffer::setTemp(u32 index, int temperatureC) {
    if (index >= this->count || temperatureC < MinTemp || temperatureC > MaxTemp || this->tempTaken(temperatureC, index)) {
        return false;
    }
    this->points[index].temperature_c = temperatureC;
    return true;
}

void CurveBuffer::setLevel(u32 index, float level) {
    if (index < this->count) {
        this->points[index].fanLevel_f = std::clamp(level, 0.0f, 1.0f);
    }
}

/* ---- AppState ---- */

void AppState::reloadProfiles() {
    EnsureProfilesInitialized();
    this->profileCount  = GetProfileIds(this->profileIds, MaxProfiles);
    this->activeProfile = GetActiveProfileId();

    if (this->profileCount == 0) {
        this->profileCursor = 0;
    } else if (this->profileCursor >= (int)this->profileCount) {
        this->profileCursor = (int)this->profileCount - 1;
    }
}

void AppState::reloadCurves() {
    this->handheld.bind(this->activeProfile, false);
    this->handheld.load();

    this->docked.bind(this->activeProfile, true);
    this->docked.load();

    const u32 count = this->currentCurve().count;
    if (this->pointCursor >= (int)count) {
        this->pointCursor = count > 0 ? (int)count - 1 : 0;
    }
}

void AppState::reloadSettings() {
    this->enabled                  = IsEnabled(ConfigSection);
    this->gameProfiles             = IsGameProfilesEnabled();
    this->fanSensor                = GetFanSensor();
    this->dockedOverride           = IsDockedOverride(ConfigSection);
    this->fastRefreshTemperatureC  = GetFastRefreshTemperatureC(ConfigSection);
    this->slowRefreshIntervalMs    = GetRefreshInterval(ConfigSection, KeySlowRefreshIntervalMs, DefaultSlowRefreshIntervalMs);
    this->fastRefreshIntervalMs    = GetRefreshInterval(ConfigSection, KeyFastRefreshIntervalMs, DefaultFastRefreshIntervalMs);
    this->configRefreshIntervalMs  = GetRefreshInterval(ConfigSection, KeyConfigRefreshIntervalMs, DefaultConfigRefreshIntervalMs);
    this->enableRefreshIntervalMs  = GetRefreshInterval(ConfigSection, KeyEnableRefreshIntervalMs, DefaultEnableRefreshIntervalMs);
    this->dockedRefreshIntervalMs  = GetRefreshInterval(ConfigSection, KeyDockedRefreshIntervalMs, DefaultDockedRefreshIntervalMs);
}

void AppState::saveSettings() {
    SetFastRefreshTemperatureC(ConfigSection, this->fastRefreshTemperatureC);
    SetRefreshInterval(ConfigSection, KeySlowRefreshIntervalMs, this->slowRefreshIntervalMs);
    SetRefreshInterval(ConfigSection, KeyFastRefreshIntervalMs, this->fastRefreshIntervalMs);
    SetRefreshInterval(ConfigSection, KeyConfigRefreshIntervalMs, this->configRefreshIntervalMs);
    SetRefreshInterval(ConfigSection, KeyEnableRefreshIntervalMs, this->enableRefreshIntervalMs);
    SetRefreshInterval(ConfigSection, KeyDockedRefreshIntervalMs, this->dockedRefreshIntervalMs);
}

void AppState::setStatus(const std::string &text) {
    this->status = text;
    /* roughly three seconds */
    this->statusUntilTick = armGetSystemTick() + armGetSystemTickFreq() * 3;
}

std::string AppState::profileName(u32 id) const {
    char name[MaxProfileNameLength + 1];
    GetProfileName(id, name, sizeof(name));
    return std::string(name);
}

bool PromptText(const char *guide, const char *initial, char *out, size_t outSize) {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) {
        return false;
    }

    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetGuideText(&kbd, guide);
    swkbdConfigSetStringLenMax(&kbd, MaxProfileNameLength);
    if (initial != nullptr && initial[0] != 0) {
        swkbdConfigSetInitialText(&kbd, initial);
    }

    const Result rc = swkbdShow(&kbd, out, outSize);
    swkbdClose(&kbd);

    return R_SUCCEEDED(rc) && out[0] != 0;
}

void AppState::reloadMappings() {
    this->mappingCount = GetTitleMappings(this->mappings, MaxTitleMappings);
    this->gameProfiles = IsGameProfilesEnabled();

    char name[0x200];
    for (u32 i = 0; i < this->mappingCount; ++i) {
        this->mappingNames[i] = GetTitleName(this->mappings[i].titleId, name, sizeof(name)) ? name : "";
    }

    if (this->mappingCount == 0) {
        this->gameCursor = 0;
    } else if (this->gameCursor >= (int)this->mappingCount) {
        this->gameCursor = (int)this->mappingCount - 1;
    }
}
