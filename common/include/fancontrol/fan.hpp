#pragma once

#include <switch.h>

struct TemperaturePoint {
    int  temperature_c;
    float fanLevel_f;
};

struct Context {
    bool enabled;
    bool dockedOverride;

    bool isDocked;

    bool gameProfiles;
    u32  sensor;
    u64  titleId;

    struct {
        u32 fastRefreshTemperatureC;
        u32 slowRefreshIntervalMs;
        u32 fastRefreshIntervalMs;
        u32 configRefreshIntervalMs;
        u32 enableRefreshIntervalMs;
        u32 dockedRefreshIntervalMs;
        u32 titleRefreshIntervalMs;
    } refreshConfig;

    TemperaturePoint *table;
    u32 tableEntries;
};

#define MAX_TABLE_ENTRIES  32

void SortFanCurveTable(TemperaturePoint *table, u32 count);
float InterpolateFanLevel(const TemperaturePoint *table, u32 count, float temperature_c);

/* ---- display helpers ----
 *
 * Everything shown to the user rounds rather than truncates, so a live 39.6C
 * reads "40C" and an interpolated level of 0.4666 reads "47%" rather than 46%.
 * Use these anywhere a float becomes text or a step index, so every screen
 * agrees with every other.
 */
int RoundToInt(float value);
int LevelToPercent(float level);
