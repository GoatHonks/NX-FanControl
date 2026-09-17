#pragma once

#include "fan.hpp"

/* Built-in curve templates.
 *
 * Stock-like is an APPROXIMATION of Nintendo's default behaviour, not a copy
 * of it. The stock table in the tc sysmodule is indexed on skin temperature,
 * while this tool drives the fan from the TMP451 SoC sensor, which runs
 * considerably hotter. The shape of the stock curve is preserved (a ~20%
 * floor, a plateau, then a ramp) and the breakpoints are shifted into SoC
 * terms; the top end ramps to 100% rather than stopping at the stock 60%,
 * because an SoC-driven curve has to cover cases the skin sensor never sees.
 */

enum CurvePreset {
    CurvePreset_Default = 0,
    CurvePreset_StockLike,
    CurvePreset_Count,
};

const char *GetPresetName(CurvePreset preset);
const char *GetPresetDescription(CurvePreset preset);

/* Fills points with the preset's curve and returns the point count. */
u32 GetPresetCurve(CurvePreset preset, bool docked, TemperaturePoint *points, u32 maxPoints);

/* Creates a new profile whose handheld and docked curves come from a preset. */
bool CreateProfileFromPreset(const char *name, CurvePreset preset, u32 *outId);

/* Overwrites an existing profile's curves with a preset. */
bool ApplyPresetToProfile(u32 profileId, CurvePreset preset);
