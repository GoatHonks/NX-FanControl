#include <fancontrol.hpp>
#include <minIni.h>
#include <algorithm>

constexpr const char *KeyCount           = "pointCount";
constexpr const char *KeyEnabled         = "enabled";
constexpr const char *KeyDockedOverride  = "docked_override";
constexpr const char *KeyFastRefreshTemp = "high_refresh_temp_c";

static int ClampSpeed(int speed) {
    if (speed < 0) {
        return 0;
    }
    if (speed > 100) {
        return 100;
    }
    return speed;
}

u32 LoadCurve(const char *section, TemperaturePoint *points, u32 maxPoints) {
    if (points == NULL) {
        return 0;
    }

    char key[16], value[32];
    u32 count = 0;

    for (u32 i = 0; i < maxPoints; ++i) {
        snprintf(key, sizeof(key), "p%u", i);
        int len = ini_gets(section, key, "", value, sizeof(value), FC_CONFIG_INI);
        if (len == 0) {
            break;
        }

        int temp;
        int speed;
        if (sscanf(value, "%dC%d%%", &temp, &speed) == 2) {
            points[count].temperature_c = temp;
            points[count].fanLevel_f    = static_cast<float>(ClampSpeed(speed)) / 100.0f;
            ++count;
        }
    }

    return count;
}

u32 GetPointCount(const char *section) {
    long n = ini_getl(section, KeyCount, 0, FC_CONFIG_INI);
    if (n < 0) {
        return 0;
    }
    if (n > MAX_TABLE_ENTRIES) {
        return MAX_TABLE_ENTRIES;
    }
    return static_cast<u32>(n);
}

bool IsEnabled(const char *section) {
    return ini_getbool(section, KeyEnabled, 0, FC_CONFIG_INI);
}

bool IsDockedOverride(const char *section) {
    return ini_getbool(section, KeyDockedOverride, 0, FC_CONFIG_INI);
}

static bool BeginConfigWrite(void) {
    if (access(FC_CONFIG_DIR, F_OK) == -1) {
        CreateDir(FC_CONFIG_DIR);
    }
    remove(FC_CONFIG_INI_TMP);

    FILE *src = fopen(FC_CONFIG_INI, "rb");
    if (src == NULL) {
        return true;
    }

    FILE *dst = fopen(FC_CONFIG_INI_TMP, "wb");
    if (dst == NULL) {
        fclose(src);
        WriteLog("BeginConfigWrite: fopen tmp failed");
        return false;
    }

    char buf[512];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            ok = false;
            break;
        }
    }

    fclose(src);
    if (fclose(dst) != 0) {
        ok = false;
    }
    if (!ok) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog("BeginConfigWrite: copy failed");
    }
    return ok;
}

static bool CommitConfigWrite(void) {
    remove(FC_CONFIG_INI);
    if (rename(FC_CONFIG_INI_TMP, FC_CONFIG_INI) != 0) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog("CommitConfigWrite: rename failed");
        return false;
    }
    return true;
}

static bool WriteConfigLong(const char *section, const char *key, long value, const char *failure) {
    if (!BeginConfigWrite()) {
        return false;
    }

    if (!ini_putl(section, key, value, FC_CONFIG_INI_TMP)) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog(failure);
        return false;
    }

    return CommitConfigWrite();
}

/* Writes a curve into the pending (tmp) config. Must run between
 * BeginConfigWrite and CommitConfigWrite. Clears any leftover points beyond
 * count so a shrinking curve does not keep stale entries. */
static bool WritePendingCurve(const char *section, const TemperaturePoint *points, u32 count, u32 oldCount) {
    char key[16], value[32];

    for (u32 i = 0; i < count; ++i) {
        int speed = ClampSpeed(static_cast<int>(points[i].fanLevel_f * 100.0f + 0.5f));
        snprintf(key, sizeof(key), "p%u", i);
        snprintf(value, sizeof(value), "%dC%d%%", points[i].temperature_c, speed);
        if (!ini_puts(section, key, value, FC_CONFIG_INI_TMP)) {
            WriteLog("WritePendingCurve: ini_puts failed");
            return false;
        }
    }

    for (u32 i = count; i < oldCount; ++i) {
        snprintf(key, sizeof(key), "p%u", i);
        ini_puts(section, key, NULL, FC_CONFIG_INI_TMP);
    }

    if (!ini_putl(section, KeyCount, count, FC_CONFIG_INI_TMP)) {
        WriteLog("WritePendingCurve: ini_putl failed");
        return false;
    }

    return true;
}

bool SaveCurve(const char *section, const TemperaturePoint *points, u32 count) {
    if (points == NULL || count == 0 || count > MAX_TABLE_ENTRIES) {
        return false;
    }

    u32 oldCount = GetPointCount(section);

    if (!BeginConfigWrite()) {
        return false;
    }

    if (!WritePendingCurve(section, points, count, oldCount)) {
        remove(FC_CONFIG_INI_TMP);
        return false;
    }

    return CommitConfigWrite();
}

bool SetEnabled(const char *section, bool enabled) {
    return WriteConfigLong(section, KeyEnabled, enabled ? 1 : 0, "SetEnabled: ini_putl failed");
}

bool SetDockedOverride(const char *section, bool enabled) {
    return WriteConfigLong(section, KeyDockedOverride, enabled ? 1 : 0, "SetDockedOverride: ini_putl failed");
}

u32 GetFastRefreshTemperatureC(const char *section) {
    return static_cast<u32>(std::max(ini_getl(section, KeyFastRefreshTemp, DefaultFastRefreshTempC, FC_CONFIG_INI), 0L));
}

bool SetFastRefreshTemperatureC(const char *section, u32 temp) {
    return WriteConfigLong(section, KeyFastRefreshTemp, static_cast<long>(temp), "SetFastRefreshTemperatureC: ini_putl failed");
}

u32 GetRefreshInterval(const char *section, const char *key, u32 defaultMs) {
    return static_cast<u32>(std::max(ini_getl(section, key, defaultMs, FC_CONFIG_INI), 0L));
}

bool SetRefreshInterval(const char *section, const char *key, u32 ms) {
    return WriteConfigLong(section, key, static_cast<long>(ms), "SetRefreshInterval: ini_putl failed");
}

/* ---- Profiles ---- */

void GetProfileSection(u32 id, bool docked, char *out, size_t outSize) {
    if (out == NULL || outSize == 0) {
        return;
    }

    /* Profile 0 lives in the original section names so that configs from
     * versions without profile support keep working as-is. */
    if (id == 0) {
        snprintf(out, outSize, "%s", docked ? DockedOverrideCurveSection : CurveSection);
        return;
    }

    if (docked) {
        snprintf(out, outSize, "profile%u_docked", id);
    } else {
        snprintf(out, outSize, "profile%u", id);
    }
}

static bool ProfileListContains(const u32 *ids, u32 count, u32 id) {
    for (u32 i = 0; i < count; ++i) {
        if (ids[i] == id) {
            return true;
        }
    }
    return false;
}

u32 GetProfileIds(u32 *ids, u32 maxIds) {
    if (ids == NULL || maxIds == 0) {
        return 0;
    }

    char buf[128];
    ini_gets(ConfigSection, KeyProfileOrder, "", buf, sizeof(buf), FC_CONFIG_INI);

    u32 count = 0;
    const char *p = buf;
    while (*p != 0 && count < maxIds) {
        char *end = NULL;
        long value = strtol(p, &end, 10);
        if (end == p) {
            /* Not a number. Skip just this entry and keep going, so one stray
             * token cannot truncate the rest of the list. */
            while (*p != 0 && *p != ',') {
                ++p;
            }
            while (*p == ',' || *p == ' ') {
                ++p;
            }
            continue;
        }

        /* Ignore out-of-range or repeated ids rather than failing outright, so
         * a hand-edited config cannot leave the user without any profiles. */
        if (value >= 0 && value < static_cast<long>(MaxProfiles) && !ProfileListContains(ids, count, static_cast<u32>(value))) {
            ids[count++] = static_cast<u32>(value);
        }

        p = end;
        while (*p == ',' || *p == ' ') {
            ++p;
        }
    }

    return count;
}

u32 GetProfileCount(void) {
    u32 ids[MaxProfiles];
    return GetProfileIds(ids, MaxProfiles);
}

bool ProfileExists(u32 id) {
    u32 ids[MaxProfiles];
    u32 count = GetProfileIds(ids, MaxProfiles);
    return ProfileListContains(ids, count, id);
}

static bool WritePendingProfileOrder(const u32 *ids, u32 count) {
    char buf[128];
    int offset = 0;

    for (u32 i = 0; i < count; ++i) {
        int written = (i == 0)
                          ? snprintf(buf + offset, sizeof(buf) - offset, "%u", ids[i])
                          : snprintf(buf + offset, sizeof(buf) - offset, ",%u", ids[i]);
        if (written < 0 || static_cast<size_t>(offset + written) >= sizeof(buf)) {
            WriteLog("WritePendingProfileOrder: order string too long");
            return false;
        }
        offset += written;
    }
    buf[offset] = 0;

    if (!ini_puts(ConfigSection, KeyProfileOrder, buf, FC_CONFIG_INI_TMP)) {
        WriteLog("WritePendingProfileOrder: ini_puts failed");
        return false;
    }
    return true;
}

u32 GetActiveProfileId(void) {
    u32 ids[MaxProfiles];
    u32 count = GetProfileIds(ids, MaxProfiles);
    if (count == 0) {
        return 0;
    }

    long active = ini_getl(ConfigSection, KeyActiveProfile, 0, FC_CONFIG_INI);
    if (active >= 0 && ProfileListContains(ids, count, static_cast<u32>(active))) {
        return static_cast<u32>(active);
    }

    /* Active profile was deleted or never valid; fall back to the first. */
    return ids[0];
}

bool SetActiveProfileId(u32 id) {
    if (!ProfileExists(id)) {
        return false;
    }
    return WriteConfigLong(ConfigSection, KeyActiveProfile, static_cast<long>(id), "SetActiveProfileId: ini_putl failed");
}

void GetProfileName(u32 id, char *out, size_t outSize) {
    if (out == NULL || outSize == 0) {
        return;
    }

    char section[ProfileSectionSize];
    GetProfileSection(id, false, section, sizeof(section));

    ini_gets(section, KeyProfileName, "", out, outSize, FC_CONFIG_INI);
    if (out[0] != 0) {
        return;
    }

    if (id == 0) {
        snprintf(out, outSize, "%s", DefaultProfileName);
    } else {
        snprintf(out, outSize, "Profile %u", id + 1);
    }
}

/* Strips characters that would corrupt the INI or produce an unreadable
 * entry, and trims surrounding whitespace. Returns false if nothing usable
 * is left. */
static bool SanitizeProfileName(const char *name, char *out, size_t outSize) {
    if (name == NULL || out == NULL || outSize == 0) {
        return false;
    }

    size_t len = 0;
    for (const char *p = name; *p != 0 && len + 1 < outSize && len < MaxProfileNameLength; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20 || c == '[' || c == ']' || c == '=' || c == ';' || c == ':' || c == '#') {
            continue;
        }
        out[len++] = static_cast<char>(c);
    }
    out[len] = 0;

    while (len > 0 && out[len - 1] == ' ') {
        out[--len] = 0;
    }

    size_t start = 0;
    while (out[start] == ' ') {
        ++start;
    }
    if (start > 0) {
        memmove(out, out + start, len - start + 1);
        len -= start;
    }

    return len > 0;
}

bool SetProfileName(u32 id, const char *name) {

    if (!ProfileExists(id)) {
        return false;
    }

    char clean[MaxProfileNameLength + 1];
    if (!SanitizeProfileName(name, clean, sizeof(clean))) {
        return false;
    }

    char section[ProfileSectionSize];
    GetProfileSection(id, false, section, sizeof(section));

    if (!BeginConfigWrite()) {
        return false;
    }

    if (!ini_puts(section, KeyProfileName, clean, FC_CONFIG_INI_TMP)) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog("SetProfileName: ini_puts failed");
        return false;
    }

    return CommitConfigWrite();
}

bool CreateProfile(const char *name, u32 seedFromId, u32 *outId) {
    u32 ids[MaxProfiles];
    u32 count = GetProfileIds(ids, MaxProfiles);
    if (count >= MaxProfiles) {
        return false;
    }

    /* Stable ids: take the lowest free slot so deleting never renumbers. */
    u32 newId = MaxProfiles;
    for (u32 candidate = 0; candidate < MaxProfiles; ++candidate) {
        if (!ProfileListContains(ids, count, candidate)) {
            newId = candidate;
            break;
        }
    }
    if (newId == MaxProfiles) {
        return false;
    }

    char clean[MaxProfileNameLength + 1];
    if (!SanitizeProfileName(name, clean, sizeof(clean))) {
        snprintf(clean, sizeof(clean), "Profile %u", newId + 1);
    }

    /* Read the seed curves before opening the write transaction, since reads
     * always come from the committed file. */
    TemperaturePoint handheld[MAX_TABLE_ENTRIES];
    TemperaturePoint docked[MAX_TABLE_ENTRIES];
    u32 handheldCount = 0;
    u32 dockedCount   = 0;

    if (ProfileListContains(ids, count, seedFromId)) {
        char seedSection[ProfileSectionSize];
        GetProfileSection(seedFromId, false, seedSection, sizeof(seedSection));
        handheldCount = LoadCurve(seedSection, handheld, MAX_TABLE_ENTRIES);

        GetProfileSection(seedFromId, true, seedSection, sizeof(seedSection));
        dockedCount = LoadCurve(seedSection, docked, MAX_TABLE_ENTRIES);
    }

    char section[ProfileSectionSize];

    if (!BeginConfigWrite()) {
        return false;
    }

    GetProfileSection(newId, false, section, sizeof(section));
    if (!ini_puts(section, KeyProfileName, clean, FC_CONFIG_INI_TMP)) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog("CreateProfile: writing name failed");
        return false;
    }

    if (handheldCount > 0 && !WritePendingCurve(section, handheld, handheldCount, 0)) {
        remove(FC_CONFIG_INI_TMP);
        return false;
    }

    if (dockedCount > 0) {
        GetProfileSection(newId, true, section, sizeof(section));
        if (!WritePendingCurve(section, docked, dockedCount, 0)) {
            remove(FC_CONFIG_INI_TMP);
            return false;
        }
    }

    ids[count++] = newId;
    if (!WritePendingProfileOrder(ids, count)) {
        remove(FC_CONFIG_INI_TMP);
        return false;
    }

    if (!CommitConfigWrite()) {
        return false;
    }

    if (outId != NULL) {
        *outId = newId;
    }
    return true;
}

bool DeleteProfile(u32 id) {
    u32 ids[MaxProfiles];
    u32 count = GetProfileIds(ids, MaxProfiles);

    /* Never leave the user with no profile at all.
     */
    if (count <= 1 || !ProfileListContains(ids, count, id)) {
        return false;
    }

    u32 remaining[MaxProfiles];
    u32 remainingCount = 0;
    for (u32 i = 0; i < count; ++i) {
        if (ids[i] != id) {
            remaining[remainingCount++] = ids[i];
        }
    }

    const bool wasActive = (GetActiveProfileId() == id);

    char section[ProfileSectionSize];

    if (!BeginConfigWrite()) {
        return false;
    }

    /* Passing a NULL key deletes the whole section. */
    GetProfileSection(id, false, section, sizeof(section));
    ini_puts(section, NULL, NULL, FC_CONFIG_INI_TMP);

    GetProfileSection(id, true, section, sizeof(section));
    ini_puts(section, NULL, NULL, FC_CONFIG_INI_TMP);

    if (!WritePendingProfileOrder(remaining, remainingCount)) {
        remove(FC_CONFIG_INI_TMP);
        return false;
    }

    if (wasActive && !ini_putl(ConfigSection, KeyActiveProfile, remaining[0], FC_CONFIG_INI_TMP)) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog("DeleteProfile: updating active profile failed");
        return false;
    }

    return CommitConfigWrite();
}

bool MoveProfile(u32 id, int delta) {
    u32 ids[MaxProfiles];
    u32 count = GetProfileIds(ids, MaxProfiles);

    u32 index = count;
    for (u32 i = 0; i < count; ++i) {
        if (ids[i] == id) {
            index = i;
            break;
        }
    }
    if (index == count) {
        return false;
    }

    long target = static_cast<long>(index) + delta;
    if (target < 0 || target >= static_cast<long>(count)) {
        return false;
    }

    u32 swap          = ids[index];
    ids[index]        = ids[target];
    ids[target]       = swap;

    if (!BeginConfigWrite()) {
        return false;
    }

    if (!WritePendingProfileOrder(ids, count)) {
        remove(FC_CONFIG_INI_TMP);
        return false;
    }

    return CommitConfigWrite();
}

void EnsureProfilesInitialized(void) {
    if (GetProfileCount() > 0) {
        return;
    }

    /* No profile list yet: adopt whatever curve already exists as profile 0.
     * This is the upgrade path from versions before profiles existed. */
    if (!BeginConfigWrite()) {
        return;
    }

    u32 ids[1] = { 0 };
    if (!WritePendingProfileOrder(ids, 1)) {
        remove(FC_CONFIG_INI_TMP);
        return;
    }

    if (!ini_putl(ConfigSection, KeyActiveProfile, 0, FC_CONFIG_INI_TMP)) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog("EnsureProfilesInitialized: ini_putl failed");
        return;
    }

    char section[ProfileSectionSize];
    GetProfileSection(0, false, section, sizeof(section));

    char existing[MaxProfileNameLength + 1];
    ini_gets(section, KeyProfileName, "", existing, sizeof(existing), FC_CONFIG_INI);
    if (existing[0] == 0) {
        ini_puts(section, KeyProfileName, DefaultProfileName, FC_CONFIG_INI_TMP);
    }

    CommitConfigWrite();
}

/* ---- Per-game profiles ----
 *
 * Mappings live in their own section as titleId=profileId, e.g.
 *   [gameProfiles]
 *   0100152000022000=2
 * The sysmodule resolves the running title against this table and falls back
 * to the manually selected profile when a game has no mapping.
 */

u32 GetTitleMappingCount(void) {
    char key[32];
    u32 count = 0;
    for (int i = 0; ini_getkey(GameProfileSection, i, key, sizeof(key), FC_CONFIG_INI) > 0; ++i) {
        ++count;
        if (count >= MaxTitleMappings) {
            break;
        }
    }
    return count;
}

u32 GetTitleMappings(TitleMapping *out, u32 maxCount) {
    if (out == NULL || maxCount == 0) {
        return 0;
    }

    char key[32];
    u32 count = 0;

    for (int i = 0; count < maxCount; ++i) {
        if (ini_getkey(GameProfileSection, i, key, sizeof(key), FC_CONFIG_INI) <= 0) {
            break;
        }

        const u64 titleId = strtoull(key, NULL, 16);
        if (titleId == 0) {
            continue;
        }

        const long profile = ini_getl(GameProfileSection, key, -1, FC_CONFIG_INI);
        if (profile < 0 || profile >= (long)MaxProfiles) {
            continue;
        }

        /* A mapping pointing at a deleted profile is stale; skip it rather
         * than handing the caller a profile that no longer exists. */
        if (!ProfileExists((u32)profile)) {
            continue;
        }

        out[count].titleId   = titleId;
        out[count].profileId = (u32)profile;
        ++count;
    }

    return count;
}

static void FormatTitleId(u64 titleId, char *out, size_t outSize) {
    snprintf(out, outSize, "%016lX", titleId);
}

bool GetProfileForTitle(u64 titleId, u32 *outProfileId) {
    if (titleId == 0) {
        return false;
    }

    char key[32];
    FormatTitleId(titleId, key, sizeof(key));

    const long profile = ini_getl(GameProfileSection, key, -1, FC_CONFIG_INI);
    if (profile < 0 || profile >= (long)MaxProfiles || !ProfileExists((u32)profile)) {
        return false;
    }

    if (outProfileId != NULL) {
        *outProfileId = (u32)profile;
    }
    return true;
}

bool SetProfileForTitle(u64 titleId, u32 profileId) {
    if (titleId == 0 || !ProfileExists(profileId)) {
        return false;
    }

    /* Replacing an existing mapping is fine; adding a new one past the cap is
     * not, otherwise the table could grow without bound. */
    if (!GetProfileForTitle(titleId, NULL) && GetTitleMappingCount() >= MaxTitleMappings) {
        return false;
    }

    char key[32];
    FormatTitleId(titleId, key, sizeof(key));

    return WriteConfigLong(GameProfileSection, key, (long)profileId, "SetProfileForTitle: ini_putl failed");
}

bool ClearProfileForTitle(u64 titleId) {
    if (titleId == 0) {
        return false;
    }

    char key[32];
    FormatTitleId(titleId, key, sizeof(key));

    if (!BeginConfigWrite()) {
        return false;
    }

    /* A NULL value deletes the key. */
    if (!ini_puts(GameProfileSection, key, NULL, FC_CONFIG_INI_TMP)) {
        remove(FC_CONFIG_INI_TMP);
        WriteLog("ClearProfileForTitle: ini_puts failed");
        return false;
    }

    return CommitConfigWrite();
}

bool IsGameProfilesEnabled(void) {
    return ini_getbool(ConfigSection, KeyGameProfiles, 0, FC_CONFIG_INI);
}

bool SetGameProfilesEnabled(bool enabled) {
    return WriteConfigLong(ConfigSection, KeyGameProfiles, enabled ? 1 : 0, "SetGameProfilesEnabled: ini_putl failed");
}

/* Resolves which profile should be in effect for a title: its own mapping if
 * per-game profiles are on and one exists, otherwise the selected profile. */
u32 ResolveProfileForTitle(u64 titleId) {
    u32 mapped = 0;
    if (IsGameProfilesEnabled() && GetProfileForTitle(titleId, &mapped)) {
        return mapped;
    }
    return GetActiveProfileId();
}

/* ---- Sensor selection ---- */

u32 GetFanSensor(void) {
    const long value = ini_getl(ConfigSection, KeyFanSensor, FanSensor_Soc, FC_CONFIG_INI);
    if (value < 0 || value >= FanSensor_Count) {
        return FanSensor_Soc;
    }
    return static_cast<u32>(value);
}

bool SetFanSensor(u32 sensor) {
    if (sensor >= FanSensor_Count) {
        return false;
    }
    return WriteConfigLong(ConfigSection, KeyFanSensor, static_cast<long>(sensor), "SetFanSensor: ini_putl failed");
}

/* ---- The Default profile ---- */

constexpr const char *KeyDefaultSeeded = "default_seeded";

void EnsureDefaultProfileIsStock(void) {
    if (GetProfileCount() == 0) {
        EnsureProfilesInitialized();
    }

    /* This runs exactly once. The Default profile is seeded with the
     * Stock-like curves and is then an ordinary editable profile: anything the
     * user changes afterwards must survive every later boot, so there is no
     * "restore" path here. */
    if (ini_getbool(ConfigSection, KeyDefaultSeeded, 0, FC_CONFIG_INI)) {
        return;
    }

    ApplyPresetToProfile(0, CurvePreset_StockLike);
    SetProfileName(0, DefaultProfileName);

    WriteConfigLong(ConfigSection, KeyDefaultSeeded, 1, "EnsureDefaultProfileIsStock: ini_putl failed");
}
