#pragma once
#include "fan.hpp"

#define FC_CONFIG_DIR     "./config/NX-FanControl/"
#define FC_CONFIG_INI     "./config/NX-FanControl/config.ini"
#define FC_CONFIG_INI_TMP "./config/NX-FanControl/config.ini.tmp"

constexpr const char *ConfigSection              = "config";
constexpr const char *CurveSection               = "defaultCurve";
constexpr const char *DockedOverrideCurveSection = "dockedCurveOverride";

constexpr const char *KeySlowRefreshIntervalMs   = "low_temp_interval_ms";
constexpr const char *KeyFastRefreshIntervalMs   = "high_temp_interval_ms";
constexpr const char *KeyConfigRefreshIntervalMs = "config_refresh_interval_ms";
constexpr const char *KeyEnableRefreshIntervalMs = "enable_refresh_interval_ms";
constexpr const char *KeyDockedRefreshIntervalMs = "docked_refresh_interval_ms";
constexpr const char *KeyTitleRefreshIntervalMs  = "title_refresh_interval_ms";

constexpr u32 MinFastRefreshTempC     = 20;
constexpr u32 MaxFastRefreshTempC     = 80;
constexpr u32 DefaultFastRefreshTempC = 55;

constexpr u32 MinPollIntervalMs  = 5;
constexpr u32 MaxPollIntervalMs  = 200;
constexpr u32 MinCheckIntervalMs = 100;
constexpr u32 MaxCheckIntervalMs = 5000;

constexpr u32 DefaultSlowRefreshIntervalMs   = 50;
constexpr u32 DefaultFastRefreshIntervalMs   = 25;
constexpr u32 DefaultConfigRefreshIntervalMs = 2000;
constexpr u32 DefaultEnableRefreshIntervalMs = 2000;
constexpr u32 DefaultDockedRefreshIntervalMs = 1000;
constexpr u32 DefaultTitleRefreshIntervalMs  = 2000;

u32 LoadCurve(const char *section, TemperaturePoint *points, u32 maxPoints);
u32 GetPointCount(const char *section);
bool IsEnabled(const char *section);

bool SaveCurve(const char *section, const TemperaturePoint *points, u32 count);
bool SetEnabled(const char *section, bool enabled);

bool IsDockedOverride(const char *section);
bool SetDockedOverride(const char *section, bool enabled);

u32 GetFastRefreshTemperatureC(const char *section);
bool SetFastRefreshTemperatureC(const char *section, u32 temp);

u32 GetRefreshInterval(const char *section, const char *key, u32 defaultMs);
bool SetRefreshInterval(const char *section, const char *key, u32 ms);

/* ---- Profiles ----
 *
 * A profile owns a pair of curve sections: a handheld curve and a docked
 * curve. Profiles are addressed by a stable id that never changes once
 * assigned, so deleting one never renumbers the others.
 *
 * Profile 0 deliberately maps onto the legacy [defaultCurve] /
 * [dockedCurveOverride] sections so that configs written by older versions
 * keep working untouched; they simply become the first profile.
 */

constexpr u32 MaxProfiles          = 8;
constexpr u32 MaxProfileNameLength = 24;
constexpr u32 ProfileSectionSize   = 32;

constexpr const char *KeyProfileOrder  = "profile_order";
constexpr const char *KeyActiveProfile = "active_profile";
constexpr const char *KeyProfileName   = "name";

constexpr const char *DefaultProfileName = "Default";

/* Writes the INI section name holding this profile's curve into out. */
void GetProfileSection(u32 id, bool docked, char *out, size_t outSize);

/* Fills ids with the profile ids in display order and returns how many. */
u32 GetProfileIds(u32 *ids, u32 maxIds);
u32 GetProfileCount(void);

u32 GetActiveProfileId(void);
bool SetActiveProfileId(u32 id);

/* Falls back to a generated name when the profile has none stored. */
void GetProfileName(u32 id, char *out, size_t outSize);
bool SetProfileName(u32 id, const char *name);

bool ProfileExists(u32 id);

/* Creates a profile seeded from seedFromId's curves, or from the built-in
 * default curve when seedFromId is not a live profile. Returns the new id. */
bool CreateProfile(const char *name, u32 seedFromId, u32 *outId);
bool DeleteProfile(u32 id);

/* Moves a profile up (-1) or down (+1) in display order. */
bool MoveProfile(u32 id, int delta);

/* Creates profile 0 from any pre-existing curve data if no profile list
 * exists yet. Safe to call on every boot. */
void EnsureProfilesInitialized(void);

/* ---- Sensor selection ---- */

constexpr const char *KeyFanSensor = "fan_sensor";

/* Which temperature source drives the curve. Defaults to the SoC sensor,
 * which is what every previous version used. */
u32 GetFanSensor(void);
bool SetFanSensor(u32 sensor);

/* ---- The Default profile ----
 *
 * Profile 0 is seeded with the Stock-like curves the first time this runs, and
 * is an ordinary editable profile from then on. Edits to it persist.
 */

/* Seeds profile 0 with the Stock-like curves, once. Safe to call on every
 * boot: after the first time it does nothing, so user edits are never
 * overwritten. */
void EnsureDefaultProfileIsStock(void);

/* ---- Per-game profiles ----
 *
 * A title id can be bound to a profile, so launching that game switches the
 * fan curve automatically. Unmapped games fall back to the selected profile.
 */

constexpr const char *GameProfileSection = "gameProfiles";
constexpr const char *KeyGameProfiles    = "game_profiles";

constexpr u32 MaxTitleMappings = 64;

struct TitleMapping {
    u64 titleId;
    u32 profileId;
};

bool IsGameProfilesEnabled(void);
bool SetGameProfilesEnabled(bool enabled);

/* Mappings whose profile no longer exists are skipped. */
u32 GetTitleMappings(TitleMapping *out, u32 maxCount);
u32 GetTitleMappingCount(void);

bool GetProfileForTitle(u64 titleId, u32 *outProfileId);
bool SetProfileForTitle(u64 titleId, u32 profileId);
bool ClearProfileForTitle(u64 titleId);

/* The profile that should be in effect for a title, honouring the master
 * toggle and falling back to the selected profile. */
u32 ResolveProfileForTitle(u64 titleId);
