#include <fancontrol.hpp>

#include <algorithm>

namespace {

    const TemperaturePoint DefaultHandheld[] = {
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

    /* Stock handheld keeps a ~20% floor, holds it well past idle, then ramps.
     * Stock stops at 60%; we continue to 100% at the top for safety. */
    const TemperaturePoint StockHandheld[] = {
        { .temperature_c = 25, .fanLevel_f = 0.20f },
        { .temperature_c = 45, .fanLevel_f = 0.20f },
        { .temperature_c = 55, .fanLevel_f = 0.30f },
        { .temperature_c = 60, .fanLevel_f = 0.40f },
        { .temperature_c = 65, .fanLevel_f = 0.55f },
        { .temperature_c = 70, .fanLevel_f = 0.70f },
        { .temperature_c = 75, .fanLevel_f = 0.85f },
        { .temperature_c = 80, .fanLevel_f = 1.00f },
    };

    /* Stock docked ramps harder and does reach 100%. */
    const TemperaturePoint StockDocked[] = {
        { .temperature_c = 25, .fanLevel_f = 0.20f },
        { .temperature_c = 45, .fanLevel_f = 0.25f },
        { .temperature_c = 55, .fanLevel_f = 0.40f },
        { .temperature_c = 60, .fanLevel_f = 0.55f },
        { .temperature_c = 65, .fanLevel_f = 0.70f },
        { .temperature_c = 70, .fanLevel_f = 0.85f },
        { .temperature_c = 75, .fanLevel_f = 1.00f },
    };

    template <size_t N>
    u32 CopyCurve(const TemperaturePoint (&src)[N], TemperaturePoint *dst, u32 maxPoints) {
        const u32 count = std::min<u32>(N, maxPoints);
        memcpy(dst, src, sizeof(TemperaturePoint) * count);
        return count;
    }

}

const char *GetPresetName(CurvePreset preset) {
    switch (preset) {
        case CurvePreset_StockLike: return "Stock-like";
        default:                    return "Default";
    }
}

const char *GetPresetDescription(CurvePreset preset) {
    switch (preset) {
        case CurvePreset_StockLike: return "Approximates Nintendo's stock behaviour";
        default:                    return "Silent until warm, then ramps";
    }
}

u32 GetPresetCurve(CurvePreset preset, bool docked, TemperaturePoint *points, u32 maxPoints) {
    if (points == NULL || maxPoints == 0) {
        return 0;
    }

    if (preset == CurvePreset_StockLike) {
        return docked ? CopyCurve(StockDocked, points, maxPoints)
                      : CopyCurve(StockHandheld, points, maxPoints);
    }

    return CopyCurve(DefaultHandheld, points, maxPoints);
}

bool ApplyPresetToProfile(u32 profileId, CurvePreset preset) {
    if (!ProfileExists(profileId)) {
        return false;
    }

    TemperaturePoint points[MAX_TABLE_ENTRIES];
    char section[ProfileSectionSize];

    u32 count = GetPresetCurve(preset, false, points, MAX_TABLE_ENTRIES);
    GetProfileSection(profileId, false, section, sizeof(section));
    if (count == 0 || !SaveCurve(section, points, count)) {
        return false;
    }

    count = GetPresetCurve(preset, true, points, MAX_TABLE_ENTRIES);
    GetProfileSection(profileId, true, section, sizeof(section));
    if (count == 0 || !SaveCurve(section, points, count)) {
        return false;
    }

    return true;
}

bool CreateProfileFromPreset(const char *name, CurvePreset preset, u32 *outId) {
    /* MaxProfiles is never a live id, so nothing is seeded from an existing
     * profile and the preset fully defines the new curves. */
    u32 newId = 0;
    if (!CreateProfile(name != NULL ? name : GetPresetName(preset), MaxProfiles, &newId)) {
        return false;
    }

    if (!ApplyPresetToProfile(newId, preset)) {
        DeleteProfile(newId);
        return false;
    }

    if (outId != NULL) {
        *outId = newId;
    }
    return true;
}
