#include <fancontrol.hpp>
#include "fancontrol.hpp"
#include "fan_hysteresis.hpp"
#include <algorithm>
#include <atomic>
#include <math.h>
#include <sys/stat.h>

Context *ctx;

#define TEMP_READ_RETRIES 3

static u64 MsToNs(u64 ms) {
    return ms * 1'000'000ULL;
}

bool ValidateFanCurveTable(const TemperaturePoint *tbl, u32 count) {
    for (u32 i = 0; i + 1 < count; i++) {
        if (tbl[i].temperature_c > tbl[i + 1].temperature_c) {
            return false;
        }
    }
    return true;
}

static bool IntervalElapsed(u64 *last, u64 intervalNs) {
    u64 now = armGetSystemTick();

    /* If the counter ever reads lower than the stamp we kept - which is what a
     * counter reset across sleep would look like - treat the interval as
     * elapsed and re-stamp, rather than subtracting into a huge value. */
    if (now < *last) {
        *last = now;
        return true;
    }

    if (armTicksToNs(now - *last) < intervalNs) {
        return false;
    }
    *last = now;
    return true;
}

static bool LoadCurveAlloc(const char *curveSection, TemperaturePoint **outTable, u32 *outCount) {
    u32 count = GetPointCount(curveSection);
    if (count == 0 || count > MAX_TABLE_ENTRIES) {
        return false;
    }

    TemperaturePoint *table = static_cast<TemperaturePoint *>(malloc(sizeof(TemperaturePoint) * count));
    if (table == nullptr) {
        return false;
    }

    if (LoadCurve(curveSection, table, count) != count) {
        free(table);
        return false;
    }

    SortFanCurveTable(table, count);

    if (!ValidateFanCurveTable(table, count)) {
        free(table);
        return false;
    }

    *outTable = table;
    *outCount = count;
    return true;
}

static bool SwapCurveTable(const char *curveSection, FanHysteresisState *fanState);

static const char *GetProfileCurve() {
    /* Resolved fresh on every call so that a profile switch - written by the
     * overlay, the manager, or by a game starting or closing - is picked up on
     * the next config reload. */
    static char section[ProfileSectionSize];
    const u32 profile = GetActiveProfileId();
    GetProfileSection(profile, ctx->isDocked, section, sizeof(section));

    /* A profile can exist without a docked curve (created from one that had
     * none). Use its handheld curve rather than failing: at start-up a failed
     * load leaves no table at all, which switches custom fan control off. */
    if (ctx->isDocked && GetPointCount(section) == 0) {
        GetProfileSection(profile, false, section, sizeof(section));
    }
    return section;
}

/* Title id meaning "not checked yet". Starting from this rather than 0 makes
 * the very first check always run, so a restore left pending by a reboot or
 * crash mid-game is applied as soon as the sysmodule is up. */
constexpr u64 TitleUnknown = ~0ULL;

static void RefreshRunningTitle(FanHysteresisState *fanState) {
    static u64 lastCheckTime = armGetSystemTick();
    if (!IntervalElapsed(&lastCheckTime, MsToNs(ctx->refreshConfig.titleRefreshIntervalMs))) {
        return;
    }

    /* With the feature off, report "no game": that restores anything still
     * pending once, and after that the title stays 0 and nothing happens. */
    const u64 titleId = ctx->gameProfiles ? GetRunningTitleId() : 0;

    /* Also act when the running game's assignment changes, so assigning,
     * reassigning or clearing a game while it is running takes effect at once
     * instead of on the next launch. A manual profile change doesn't touch the
     * assignment, so it still isn't overridden. */
    s32 mapping = -1;
    u32 mapped  = 0;
    if (titleId != 0 && GetProfileForTitle(titleId, &mapped)) {
        mapping = static_cast<s32>(mapped);
    }

    if (titleId == ctx->titleId && mapping == ctx->titleMapping) {
        return;
    }
    ctx->titleId      = titleId;
    ctx->titleMapping = mapping;

    if (ApplyGameProfileForTitle(titleId)) {
        SwapCurveTable(GetProfileCurve(), fanState);
        WriteLog(titleId != 0 ? "Game started, switched to its profile" : "Game closed, restored profile");
    }
}

static bool SwapCurveTable(const char *curveSection, FanHysteresisState *fanState) {
    TemperaturePoint *newTable = nullptr;
    u32 newCount = 0;
    if (!LoadCurveAlloc(curveSection, &newTable, &newCount)) {
        return false;
    }

    TemperaturePoint *oldTable = ctx->table;
    ctx->table        = newTable;
    ctx->tableEntries = newCount;
    free(oldTable);
    RebindFanHysteresisTable(fanState, ctx->table, ctx->tableEntries);
    return true;
}

static void LoadConfig() {
    ctx->enabled                               = IsEnabled(ConfigSection);
    ctx->refreshConfig.fastRefreshTemperatureC = std::clamp(GetFastRefreshTemperatureC(ConfigSection), MinFastRefreshTempC, MaxFastRefreshTempC);
    ctx->refreshConfig.slowRefreshIntervalMs   = std::max(GetRefreshInterval(ConfigSection, KeySlowRefreshIntervalMs, DefaultSlowRefreshIntervalMs), MinPollIntervalMs);
    ctx->refreshConfig.fastRefreshIntervalMs   = std::max(GetRefreshInterval(ConfigSection, KeyFastRefreshIntervalMs, DefaultFastRefreshIntervalMs), MinPollIntervalMs);
    ctx->refreshConfig.configRefreshIntervalMs = std::max(GetRefreshInterval(ConfigSection, KeyConfigRefreshIntervalMs, DefaultConfigRefreshIntervalMs), MinCheckIntervalMs);
    ctx->refreshConfig.enableRefreshIntervalMs = std::max(GetRefreshInterval(ConfigSection, KeyEnableRefreshIntervalMs, DefaultEnableRefreshIntervalMs), MinCheckIntervalMs);
    ctx->refreshConfig.dockedRefreshIntervalMs = std::max(GetRefreshInterval(ConfigSection, KeyDockedRefreshIntervalMs, DefaultDockedRefreshIntervalMs), MinCheckIntervalMs);
    ctx->refreshConfig.titleRefreshIntervalMs  = std::max(GetRefreshInterval(ConfigSection, KeyTitleRefreshIntervalMs, DefaultTitleRefreshIntervalMs), MinCheckIntervalMs);
    ctx->gameProfiles                          = IsGameProfilesEnabled();
    ctx->sensor                                = GetFanSensor();
}

void InitContext(Context *_ctx) {
    ctx = _ctx;

    ctx->table          = nullptr;
    ctx->tableEntries   = 0;

    /* Adopts any pre-profile config as profile 0 on first boot after upgrade. */
    EnsureProfilesInitialized();

    /* Seeds the Default profile with the Stock-like curves, once. */
    EnsureDefaultProfileIsStock();

    ctx->titleId        = TitleUnknown;
    ctx->titleMapping   = -1;
    ctx->dockedOverride = IsDockedOverride(ConfigSection);
    ctx->isDocked       = ctx->dockedOverride && IsDocked();

    LoadConfig();

    TemperaturePoint *table = nullptr;
    u32 count = 0;
    if (!LoadCurveAlloc(GetProfileCurve(), &table, &count)) {
        WriteLog("No valid curve at init, starting disabled");
        return;
    }

    ctx->table        = table;
    ctx->tableEntries = count;
}

static bool GetConfigStat(const char *path, time_t *mtime, off_t *size) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    *mtime = st.st_mtime;
    *size  = st.st_size;
    return true;
}

static time_t GetConfigMTime(const char *path) {
    time_t mtime = 0;
    off_t  size  = 0;
    return GetConfigStat(path, &mtime, &size) ? mtime : 0;
}

static bool TryReloadConfig(const char *configSection, FanHysteresisState *fanState, time_t &lastMTime, u32 &lastRevision) {
    time_t mtime = 0;
    off_t  size  = 0;
    if (!GetConfigStat(FC_CONFIG_INI, &mtime, &size) || mtime == 0) {
        return false;
    }
    (void) size;

    /* A change shows as a new revision (every save by this tool bumps it) or a
     * new timestamp (catches edits made by hand). The revision is what makes
     * this reliable: FAT32 timestamps move in 2-second steps, so a second quick
     * save can keep the same timestamp and would otherwise be missed. */
    const u32 revision = GetConfigRevision();
    if (mtime == lastMTime && revision == lastRevision) {
        return false;
    }
    lastMTime    = mtime;
    lastRevision = revision;

    ctx->dockedOverride = IsDockedOverride(configSection);
    ctx->isDocked       = ctx->dockedOverride && IsDocked();
    LoadConfig();

    if (!SwapCurveTable(GetProfileCurve(), fanState)) {
        static u32 lastBadRevision = ~0U;
        if (revision != lastBadRevision) {
            WriteLog("Config reload failed, keeping current curve");
            lastBadRevision = revision;
        }
        return false;
    }

    return true;
}

static void RefreshConfig(const char *configSection, FanHysteresisState *fanState) {
    static time_t lastCfgMTime   = GetConfigMTime(FC_CONFIG_INI);
    static u32    lastRevision   = GetConfigRevision();
    static u64    lastCheckTime  = armGetSystemTick();

    if (IntervalElapsed(&lastCheckTime, MsToNs(ctx->refreshConfig.configRefreshIntervalMs))) {
        if (TryReloadConfig(configSection, fanState, lastCfgMTime, lastRevision)) {
            WriteLog("Config reloaded");
        }
    }
}

static bool HasDockChanged(bool newState) {
    return newState != ctx->isDocked;
}

static void HandleDockRefresh(FanHysteresisState *fanState) {
    bool docked = IsDocked();

    if (HasDockChanged(docked)) {
        ctx->isDocked = docked;
        SwapCurveTable(GetProfileCurve(), fanState);
    }
}

static void RefreshDockedState(FanHysteresisState *fanState) {
    if (!ctx->dockedOverride) {
        return;
    }

    static u64 lastCheckTime = armGetSystemTick();

    if (IntervalElapsed(&lastCheckTime, MsToNs(ctx->refreshConfig.dockedRefreshIntervalMs))) {
        HandleDockRefresh(fanState);
    }
}

void LoopFanController() {
    FanController fc;
    float tempC = 0.0f;

    Result rs = fanOpenController(&fc, 0x3D000001);
    if (R_FAILED(rs)) {
        WriteLog("Error opening fanController");
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_ShouldNotHappen));
    }

    FanHysteresisState fanState{};
    InitFanHysteresis(&fanState, ctx->table, ctx->tableEntries, 2.0f);

    for (;;) {
        RefreshConfig(ConfigSection, &fanState);
        RefreshDockedState(&fanState);
        RefreshRunningTitle(&fanState);

        if (!ctx->enabled || ctx->table == nullptr) {
            svcSleepThread(MsToNs(ctx->refreshConfig.enableRefreshIntervalMs));
            continue;
        }

        bool readOk = false;
        for (u32 retry = 0; retry < TEMP_READ_RETRIES; ++retry) {
            if (ReadSensor(static_cast<FanSensor>(ctx->sensor), &tempC)) {
                readOk = true;
                break;
            }
        }

        /* Fall back to the SoC sensor before giving up, so a configured source
         * that stops responding cannot strand the fan. The log only records
         * changes of state: this loop runs every 25-50ms, and writing on every
         * pass would flood the SD card. */
        enum ReadState { Read_Ok, Read_FellBack, Read_Failed };
        static ReadState lastState = Read_Ok;
        ReadState state = Read_Ok;

        if (!readOk) {
            if (ReadSensor(FanSensor_Soc, &tempC)) {
                state = Read_FellBack;
            } else {
                state = Read_Failed;
                tempC = 70.0f;
            }
        }

        if (state != lastState) {
            switch (state) {
                case Read_FellBack: WriteLog("Configured sensor unavailable, using SoC"); break;
                case Read_Failed:   WriteLog("All temperature reads failed"); break;
                default:            WriteLog("Configured sensor readable again"); break;
            }
            lastState = state;
        }

        float target = UpdateFanHysteresis(&fanState, tempC);

        rs = fanControllerSetRotationSpeedLevel(&fc, target);
        if (R_FAILED(rs)) {
            WriteLog("fanControllerSetRotationSpeedLevel error");
            diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_ShouldNotHappen));
        }

        u32 intervalMs = (tempC >= ctx->refreshConfig.fastRefreshTemperatureC) ? ctx->refreshConfig.fastRefreshIntervalMs : ctx->refreshConfig.slowRefreshIntervalMs;
        svcSleepThread(MsToNs(intervalMs));
    }
}

/* This should never be called, but just in case */
void CleanupFanController() {
    if (ctx == nullptr) {
        return;
    }
    free(ctx->table);
    ctx->table = nullptr;
}
