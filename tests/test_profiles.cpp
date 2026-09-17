/* Host-side tests for the config, profile, preset, per-game and
 * sensor-selection layers. Run with tests/run.sh. */

#include <fancontrol.hpp>
#include <minIni.h>

extern "C" {
#include <hocclk/client/ipc.h>
}

#include <string>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Stubs for the platform helpers the config and sensor layers depend on.
 * The hardware reads always fail here; these tests exercise selection and
 * bookkeeping logic, not the drivers. */
void CreateDir(const char *dir) { mkdir(dir, 0777); }
void WriteLog(const char *buffer) { (void)buffer; }
void InitLog(void) {}

Result I2cReadRegHandler8(u8 reg, I2cDevice dev, u8 *out) {
    (void)reg; (void)dev; (void)out;
    return 1;
}
Result I2cReadRegHandler16(u8 reg, I2cDevice dev, u16 *out) {
    (void)reg; (void)dev; (void)out;
    return 1;
}
Result tcInitialize(void) { return 1; }
void   tcExit(void) {}
Result tcGetSkinTemperatureMilliC(s32 *skinTemp) { (void)skinTemp; return 1; }

/* Horizon OC is treated as absent here, which is the case these tests care
 * about: the CPU/GPU/RAM sensors must degrade cleanly without it. */
extern "C" {
Result hocclkIpcInitialize(void) { return 1; }
void   hocclkIpcExit(void) {}
Result hocclkIpcGetCurrentContext(HocClkContext *out_context) { (void)out_context; return 1; }
}

u64 armGetSystemTick(void)     { return 0; }
u64 armGetSystemTickFreq(void) { return 19200000; }
u64 armTicksToNs(u64 ticks)    { return ticks * 1000ULL / 19ULL; }

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg, ...)                                              \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            printf("  FAIL: " msg "\n", ##__VA_ARGS__);                    \
        }                                                                  \
    } while (0)

static void ResetConfig(void) {
    remove(FC_CONFIG_INI);
    remove(FC_CONFIG_INI_TMP);
    mkdir("./config", 0777);
    mkdir(FC_CONFIG_DIR, 0777);
}

static std::string NameOf(u32 id) {
    char buf[MaxProfileNameLength + 1];
    GetProfileName(id, buf, sizeof(buf));
    return std::string(buf);
}

static std::string SectionOf(u32 id, bool docked) {
    char buf[ProfileSectionSize];
    GetProfileSection(id, docked, buf, sizeof(buf));
    return std::string(buf);
}

static void WriteSampleCurve(const char *section, int base) {
    TemperaturePoint pts[3];
    pts[0].temperature_c = 30; pts[0].fanLevel_f = base / 100.0f;
    pts[1].temperature_c = 50; pts[1].fanLevel_f = (base + 20) / 100.0f;
    pts[2].temperature_c = 70; pts[2].fanLevel_f = (base + 40) / 100.0f;
    SaveCurve(section, pts, 3);
}

/* ---- tests ---- */

static void TestSectionMapping(void) {
    printf("section mapping\n");
    CHECK(SectionOf(0, false) == CurveSection, "profile 0 handheld should map to legacy section, got '%s'", SectionOf(0, false).c_str());
    CHECK(SectionOf(0, true) == DockedOverrideCurveSection, "profile 0 docked should map to legacy docked section, got '%s'", SectionOf(0, true).c_str());
    CHECK(SectionOf(3, false) == "profile3", "got '%s'", SectionOf(3, false).c_str());
    CHECK(SectionOf(3, true) == "profile3_docked", "got '%s'", SectionOf(3, true).c_str());
}

static void TestFreshInit(void) {
    printf("fresh init\n");
    ResetConfig();

    CHECK(GetProfileCount() == 0, "no profiles before init");
    EnsureProfilesInitialized();
    CHECK(GetProfileCount() == 1, "one profile after init, got %u", GetProfileCount());
    CHECK(GetActiveProfileId() == 0, "profile 0 active");
    CHECK(NameOf(0) == "Default", "default name, got '%s'", NameOf(0).c_str());
}

static void TestLegacyMigration(void) {
    printf("legacy migration (upgrade path)\n");
    ResetConfig();

    /* Simulate a config written by a version with no profile support. */
    WriteSampleCurve(CurveSection, 10);
    WriteSampleCurve(DockedOverrideCurveSection, 40);
    SetEnabled(ConfigSection, true);

    EnsureProfilesInitialized();

    CHECK(GetProfileCount() == 1, "legacy config becomes exactly one profile, got %u", GetProfileCount());
    CHECK(GetActiveProfileId() == 0, "legacy profile is active");

    TemperaturePoint pts[MAX_TABLE_ENTRIES];
    u32 n = LoadCurve(SectionOf(0, false).c_str(), pts, MAX_TABLE_ENTRIES);
    CHECK(n == 3, "handheld curve preserved, got %u points", n);
    CHECK(pts[0].temperature_c == 30, "first point temp preserved, got %d", pts[0].temperature_c);
    CHECK(pts[2].temperature_c == 70, "last point temp preserved, got %d", pts[2].temperature_c);

    u32 dn = LoadCurve(SectionOf(0, true).c_str(), pts, MAX_TABLE_ENTRIES);
    CHECK(dn == 3, "docked curve preserved, got %u points", dn);

    CHECK(IsEnabled(ConfigSection), "enabled flag survives migration");
}

static void TestCreateAndSeed(void) {
    printf("create + seed from source profile\n");
    ResetConfig();
    EnsureProfilesInitialized();
    WriteSampleCurve(SectionOf(0, false).c_str(), 10);
    WriteSampleCurve(SectionOf(0, true).c_str(), 55);

    u32 newId = 99;
    CHECK(CreateProfile("Quiet", 0, &newId), "create succeeds");
    CHECK(newId == 1, "second profile gets id 1, got %u", newId);
    CHECK(GetProfileCount() == 2, "two profiles now, got %u", GetProfileCount());
    CHECK(NameOf(1) == "Quiet", "name stored, got '%s'", NameOf(1).c_str());

    TemperaturePoint pts[MAX_TABLE_ENTRIES];
    u32 n = LoadCurve(SectionOf(1, false).c_str(), pts, MAX_TABLE_ENTRIES);
    CHECK(n == 3, "handheld curve seeded, got %u", n);
    CHECK(pts[0].fanLevel_f > 0.09f && pts[0].fanLevel_f < 0.11f, "seeded level copied, got %f", pts[0].fanLevel_f);

    u32 dn = LoadCurve(SectionOf(1, true).c_str(), pts, MAX_TABLE_ENTRIES);
    CHECK(dn == 3, "docked curve seeded too, got %u", dn);

    /* Creating a profile must not disturb the original. */
    u32 on = LoadCurve(SectionOf(0, false).c_str(), pts, MAX_TABLE_ENTRIES);
    CHECK(on == 3, "source profile intact, got %u", on);
}

static void TestRename(void) {
    printf("rename\n");
    ResetConfig();
    EnsureProfilesInitialized();

    u32 p = 0;
    CHECK(CreateProfile("Scratch", 0, &p), "create an editable profile");

    CHECK(SetProfileName(p, "Performance"), "rename succeeds");
    CHECK(NameOf(p) == "Performance", "renamed, got '%s'", NameOf(p).c_str());

    /* Renaming must not destroy the curve stored in the same section. */
    WriteSampleCurve(SectionOf(p, false).c_str(), 10);
    SetProfileName(p, "Balanced");
    TemperaturePoint pts[MAX_TABLE_ENTRIES];
    CHECK(LoadCurve(SectionOf(p, false).c_str(), pts, MAX_TABLE_ENTRIES) == 3, "curve survives rename");
    CHECK(NameOf(p) == "Balanced", "renamed again, got '%s'", NameOf(p).c_str());

    CHECK(!SetProfileName(p, ""), "empty name rejected");
    CHECK(!SetProfileName(p, "   "), "whitespace-only name rejected");
    CHECK(NameOf(p) == "Balanced", "name unchanged after rejection, got '%s'", NameOf(p).c_str());

    /* INI-breaking characters must be stripped, not written through. */
    CHECK(SetProfileName(p, "  [Cool]=Fast;  "), "sanitized name accepted");
    CHECK(NameOf(p) == "CoolFast", "name sanitized and trimmed, got '%s'", NameOf(p).c_str());

    CHECK(SetProfileName(p, "ThisNameIsWayTooLongToFitInTheBuffer"), "long name accepted");
    CHECK(NameOf(p).size() <= MaxProfileNameLength, "long name truncated to %u, got %zu", MaxProfileNameLength, NameOf(p).size());

    /* The config must still be parseable after a hostile name. */
    CHECK(GetProfileCount() == 2, "config still parses after sanitization, got %u", GetProfileCount());
}

static void TestActiveProfile(void) {
    printf("active profile\n");
    ResetConfig();
    EnsureProfilesInitialized();
    u32 id1 = 0, id2 = 0;
    CreateProfile("A", 0, &id1);
    CreateProfile("B", 0, &id2);

    CHECK(SetActiveProfileId(id2), "switch active");
    CHECK(GetActiveProfileId() == id2, "active is %u, got %u", id2, GetActiveProfileId());

    CHECK(!SetActiveProfileId(7), "cannot activate a nonexistent profile");
    CHECK(GetActiveProfileId() == id2, "active unchanged after bad set");
}

static void TestDelete(void) {
    printf("delete\n");
    ResetConfig();
    EnsureProfilesInitialized();
    u32 a = 0, b = 0;
    CreateProfile("A", 0, &a);
    CreateProfile("B", 0, &b);
    CHECK(GetProfileCount() == 3, "three profiles, got %u", GetProfileCount());

    SetActiveProfileId(a);
    CHECK(DeleteProfile(a), "delete active profile");
    CHECK(GetProfileCount() == 2, "two left, got %u", GetProfileCount());
    CHECK(GetActiveProfileId() != a, "active moved off deleted profile");
    CHECK(ProfileExists(GetActiveProfileId()), "active profile still exists");

    /* Deleting must remove the curve data, not orphan it. */
    CHECK(GetPointCount(SectionOf(a, false).c_str()) == 0, "deleted profile's curve removed");

    CHECK(DeleteProfile(b), "delete another");
    CHECK(GetProfileCount() == 1, "one left, got %u", GetProfileCount());
    CHECK(!DeleteProfile(GetActiveProfileId()), "refuses to delete the last profile");
    CHECK(GetProfileCount() == 1, "still one, got %u", GetProfileCount());
}

static void TestStableIds(void) {
    printf("stable ids (no renumbering)\n");
    ResetConfig();
    EnsureProfilesInitialized();
    u32 a = 0, b = 0, c = 0;
    CreateProfile("A", 0, &a);   /* 1 */
    CreateProfile("B", 0, &b);   /* 2 */
    CreateProfile("C", 0, &c);   /* 3 */
    SetProfileName(c, "Cee");

    CHECK(DeleteProfile(b), "delete middle profile");
    CHECK(NameOf(c) == "Cee", "surviving profile keeps its name (not renumbered), got '%s'", NameOf(c).c_str());
    CHECK(ProfileExists(c), "surviving profile keeps its id");

    /* The freed slot should be reused. */
    u32 d = 0;
    CHECK(CreateProfile("D", 0, &d), "create after delete");
    CHECK(d == b, "lowest free id reused: expected %u, got %u", b, d);
    CHECK(NameOf(c) == "Cee", "still intact after reuse, got '%s'", NameOf(c).c_str());
}

static void TestReorder(void) {
    printf("reorder\n");
    ResetConfig();
    EnsureProfilesInitialized();
    u32 a = 0, b = 0;
    CreateProfile("A", 0, &a);
    CreateProfile("B", 0, &b);

    u32 ids[MaxProfiles];
    u32 n = GetProfileIds(ids, MaxProfiles);
    CHECK(n == 3 && ids[0] == 0 && ids[1] == a && ids[2] == b, "initial order 0,%u,%u", a, b);

    CHECK(MoveProfile(b, -1), "move up");
    n = GetProfileIds(ids, MaxProfiles);
    CHECK(ids[1] == b && ids[2] == a, "order after move up: got %u,%u", ids[1], ids[2]);

    CHECK(MoveProfile(b, 1), "move back down");
    n = GetProfileIds(ids, MaxProfiles);
    CHECK(ids[1] == a && ids[2] == b, "order restored: got %u,%u", ids[1], ids[2]);

    CHECK(!MoveProfile(ids[0], -1), "cannot move first profile up");
    CHECK(!MoveProfile(ids[n - 1], 1), "cannot move last profile down");
}

static void TestMaxProfiles(void) {
    printf("profile limit\n");
    ResetConfig();
    EnsureProfilesInitialized();

    for (u32 i = 1; i < MaxProfiles; ++i) {
        u32 id = 0;
        CHECK(CreateProfile("X", 0, &id), "create profile %u", i);
    }
    CHECK(GetProfileCount() == MaxProfiles, "at limit, got %u", GetProfileCount());

    u32 overflow = 0;
    CHECK(!CreateProfile("TooMany", 0, &overflow), "refuses to exceed the limit");
    CHECK(GetProfileCount() == MaxProfiles, "count unchanged, got %u", GetProfileCount());
}

static void TestCorruptOrder(void) {
    printf("hand-edited / corrupt config\n");
    ResetConfig();
    EnsureProfilesInitialized();

    /* Garbage, duplicates and out-of-range ids must not wedge the parser. */
    ini_puts(ConfigSection, KeyProfileOrder, "0,0,99,-3,1,abc,2", FC_CONFIG_INI);
    u32 ids[MaxProfiles];
    u32 n = GetProfileIds(ids, MaxProfiles);
    CHECK(n == 3, "duplicates and out-of-range dropped, got %u", n);
    CHECK(ids[0] == 0 && ids[1] == 1 && ids[2] == 2, "kept 0,1,2; got %u,%u,%u", ids[0], ids[1], ids[2]);

    /* An active id that is not in the list must fall back to a real one. */
    ini_putl(ConfigSection, KeyActiveProfile, 5, FC_CONFIG_INI);
    CHECK(ProfileExists(GetActiveProfileId()), "active falls back to a live profile");
    CHECK(GetActiveProfileId() == 0, "falls back to first, got %u", GetActiveProfileId());
}

static void TestCurveRoundTrip(void) {
    printf("curve round-trip through a profile\n");
    ResetConfig();
    EnsureProfilesInitialized();
    u32 id = 0;
    CreateProfile("RT", 0, &id);

    TemperaturePoint in[4];
    in[0].temperature_c = 25; in[0].fanLevel_f = 0.00f;
    in[1].temperature_c = 40; in[1].fanLevel_f = 0.25f;
    in[2].temperature_c = 55; in[2].fanLevel_f = 0.60f;
    in[3].temperature_c = 70; in[3].fanLevel_f = 1.00f;
    CHECK(SaveCurve(SectionOf(id, false).c_str(), in, 4), "save curve");

    TemperaturePoint out[MAX_TABLE_ENTRIES];
    u32 n = LoadCurve(SectionOf(id, false).c_str(), out, MAX_TABLE_ENTRIES);
    CHECK(n == 4, "4 points back, got %u", n);
    for (u32 i = 0; i < n && i < 4; ++i) {
        CHECK(out[i].temperature_c == in[i].temperature_c, "point %u temp: want %d got %d", i, in[i].temperature_c, out[i].temperature_c);
        const float d = out[i].fanLevel_f - in[i].fanLevel_f;
        CHECK(d < 0.011f && d > -0.011f, "point %u level: want %f got %f", i, in[i].fanLevel_f, out[i].fanLevel_f);
    }

    /* Shrinking the curve must not leave stale trailing points behind. */
    CHECK(SaveCurve(SectionOf(id, false).c_str(), in, 2), "save shorter curve");
    n = LoadCurve(SectionOf(id, false).c_str(), out, MAX_TABLE_ENTRIES);
    CHECK(n == 2, "shrunk to 2, got %u", n);
}

static void TestGameMappings(void) {
    printf("per-game mappings\n");
    ResetConfig();
    EnsureProfilesInitialized();
    u32 quiet = 0, perf = 0;
    CreateProfile("Quiet", 0, &quiet);
    CreateProfile("Perf", 0, &perf);

    const u64 zelda = 0x01007EF00011E000ULL;
    const u64 mario = 0x0100000000010000ULL;

    CHECK(GetTitleMappingCount() == 0, "no mappings initially, got %u", GetTitleMappingCount());
    CHECK(!GetProfileForTitle(zelda, NULL), "unmapped title reports no profile");

    CHECK(SetProfileForTitle(zelda, quiet), "bind a title");
    u32 got = 99;
    CHECK(GetProfileForTitle(zelda, &got), "mapped title found");
    CHECK(got == quiet, "bound to %u, got %u", quiet, got);
    CHECK(GetTitleMappingCount() == 1, "one mapping, got %u", GetTitleMappingCount());

    CHECK(SetProfileForTitle(mario, perf), "bind a second title");
    CHECK(GetTitleMappingCount() == 2, "two mappings, got %u", GetTitleMappingCount());

    /* Rebinding replaces rather than duplicating. */
    CHECK(SetProfileForTitle(zelda, perf), "rebind");
    CHECK(GetTitleMappingCount() == 2, "still two mappings, got %u", GetTitleMappingCount());
    CHECK(GetProfileForTitle(zelda, &got) && got == perf, "rebound to %u, got %u", perf, got);

    /* Enumeration should return both, with correct ids. */
    TitleMapping maps[MaxTitleMappings];
    u32 n = GetTitleMappings(maps, MaxTitleMappings);
    CHECK(n == 2, "enumerated 2, got %u", n);
    bool sawZelda = false, sawMario = false;
    for (u32 i = 0; i < n; ++i) {
        if (maps[i].titleId == zelda) sawZelda = true;
        if (maps[i].titleId == mario) sawMario = true;
    }
    CHECK(sawZelda && sawMario, "both title ids round-tripped through hex");

    CHECK(ClearProfileForTitle(zelda), "clear a mapping");
    CHECK(!GetProfileForTitle(zelda, NULL), "cleared title unmapped");
    CHECK(GetTitleMappingCount() == 1, "one mapping left, got %u", GetTitleMappingCount());

    CHECK(!SetProfileForTitle(mario, 7), "cannot bind to a nonexistent profile");
    CHECK(!SetProfileForTitle(0, quiet), "cannot bind title id zero");
}

static void TestResolveAndStaleMappings(void) {
    printf("profile resolution + stale mappings\n");
    ResetConfig();
    EnsureProfilesInitialized();
    u32 quiet = 0;
    CreateProfile("Quiet", 0, &quiet);
    SetActiveProfileId(0);

    const u64 game = 0x0100ABCDEF012000ULL;
    SetProfileForTitle(game, quiet);

    /* Master toggle off: mapping is ignored. */
    SetGameProfilesEnabled(false);
    CHECK(!IsGameProfilesEnabled(), "toggle reads back false");
    CHECK(ResolveProfileForTitle(game) == 0, "falls back to selected profile when disabled, got %u", ResolveProfileForTitle(game));

    SetGameProfilesEnabled(true);
    CHECK(IsGameProfilesEnabled(), "toggle reads back true");
    CHECK(ResolveProfileForTitle(game) == quiet, "uses mapping when enabled, got %u", ResolveProfileForTitle(game));
    CHECK(ResolveProfileForTitle(0) == 0, "no running title falls back to selected");
    CHECK(ResolveProfileForTitle(0x999) == 0, "unmapped title falls back to selected");

    /* Deleting the bound profile must not strand the game on a dead id. */
    DeleteProfile(quiet);
    CHECK(!GetProfileForTitle(game, NULL), "mapping to deleted profile is not reported");
    CHECK(ProfileExists(ResolveProfileForTitle(game)), "resolution still yields a live profile");

    TitleMapping maps[MaxTitleMappings];
    CHECK(GetTitleMappings(maps, MaxTitleMappings) == 0, "stale mapping omitted from enumeration");
}

static void TestPresets(void) {
    printf("curve presets\n");
    ResetConfig();
    EnsureProfilesInitialized();

    for (int p = 0; p < CurvePreset_Count; ++p) {
        for (int d = 0; d < 2; ++d) {
            TemperaturePoint pts[MAX_TABLE_ENTRIES];
            const u32 n = GetPresetCurve((CurvePreset)p, d == 1, pts, MAX_TABLE_ENTRIES);
            CHECK(n >= 2, "preset %d docked=%d has >= 2 points, got %u", p, d, n);

            for (u32 i = 0; i < n; ++i) {
                CHECK(pts[i].temperature_c >= 20 && pts[i].temperature_c <= 80,
                      "preset %d point %u temp in range, got %d", p, i, pts[i].temperature_c);
                CHECK(pts[i].temperature_c % 5 == 0,
                      "preset %d point %u temp on a 5C step, got %d", p, i, pts[i].temperature_c);
                CHECK(pts[i].fanLevel_f >= 0.0f && pts[i].fanLevel_f <= 1.0f,
                      "preset %d point %u level in range, got %f", p, i, pts[i].fanLevel_f);
            }
            for (u32 i = 0; i + 1 < n; ++i) {
                CHECK(pts[i].temperature_c < pts[i + 1].temperature_c,
                      "preset %d temps strictly increasing at %u", p, i);
                CHECK(pts[i].fanLevel_f <= pts[i + 1].fanLevel_f,
                      "preset %d never ramps down at %u", p, i);
            }
        }
    }

    /* Stock-like should keep a non-zero floor; that is the whole point of it. */
    TemperaturePoint stock[MAX_TABLE_ENTRIES];
    u32 n = GetPresetCurve(CurvePreset_StockLike, false, stock, MAX_TABLE_ENTRIES);
    CHECK(stock[0].fanLevel_f > 0.0f, "stock-like starts above 0%%, got %f", stock[0].fanLevel_f);
    CHECK(stock[n - 1].fanLevel_f >= 0.99f, "stock-like reaches 100%% at the top, got %f", stock[n - 1].fanLevel_f);

    u32 id = 0;
    CHECK(CreateProfileFromPreset("Stocky", CurvePreset_StockLike, &id), "create from preset");
    CHECK(NameOf(id) == "Stocky", "named, got '%s'", NameOf(id).c_str());

    TemperaturePoint got[MAX_TABLE_ENTRIES];
    const u32 hn = LoadCurve(SectionOf(id, false).c_str(), got, MAX_TABLE_ENTRIES);
    CHECK(hn == n, "handheld preset written, want %u got %u", n, hn);
    CHECK(got[0].fanLevel_f > 0.0f, "written curve keeps the floor, got %f", got[0].fanLevel_f);

    const u32 dn = LoadCurve(SectionOf(id, true).c_str(), got, MAX_TABLE_ENTRIES);
    CHECK(dn >= 2, "docked preset written, got %u", dn);

    /* Applying over an existing profile replaces its curves. */
    CHECK(ApplyPresetToProfile(0, CurvePreset_StockLike), "apply preset to existing profile");
    const u32 an = LoadCurve(SectionOf(0, false).c_str(), got, MAX_TABLE_ENTRIES);
    CHECK(an == n, "existing profile overwritten, want %u got %u", n, an);
    CHECK(!ApplyPresetToProfile(7, CurvePreset_StockLike), "cannot apply to a nonexistent profile");
}

static bool CurvesEqual(const char *section, CurvePreset preset, bool docked) {
    TemperaturePoint want[MAX_TABLE_ENTRIES];
    TemperaturePoint got[MAX_TABLE_ENTRIES];
    const u32 wn = GetPresetCurve(preset, docked, want, MAX_TABLE_ENTRIES);
    const u32 gn = LoadCurve(section, got, MAX_TABLE_ENTRIES);
    if (wn != gn) {
        return false;
    }
    for (u32 i = 0; i < wn; ++i) {
        if (want[i].temperature_c != got[i].temperature_c) {
            return false;
        }
        const float d = want[i].fanLevel_f - got[i].fanLevel_f;
        if (d > 0.011f || d < -0.011f) {
            return false;
        }
    }
    return true;
}

static void TestDefaultBecomesStock(void) {
    printf("default is replaced with stock, no extra profile\n");
    ResetConfig();

    /* A pre-upgrade config: some other curve lives in profile 0. */
    WriteSampleCurve(CurveSection, 35);
    WriteSampleCurve(DockedOverrideCurveSection, 45);
    EnsureProfilesInitialized();
    CHECK(GetProfileCount() == 1, "one profile before conversion, got %u", GetProfileCount());

    const u64 game = 0x0100AAAABBBB0000ULL;
    SetGameProfilesEnabled(true);
    SetProfileForTitle(game, 0);

    EnsureDefaultProfileIsStock();

    /* The old curve is simply discarded; no profile is created for it. */
    CHECK(GetProfileCount() == 1, "no extra profile created, got %u", GetProfileCount());
    CHECK(CurvesEqual(SectionOf(0, false).c_str(), CurvePreset_StockLike, false), "default handheld is stock-like");
    CHECK(CurvesEqual(SectionOf(0, true).c_str(), CurvePreset_StockLike, true), "default docked is stock-like");
    CHECK(GetActiveProfileId() == 0, "default stays active, got %u", GetActiveProfileId());

    /* Existing bindings keep pointing at the profile they named. */
    u32 bound = 99;
    CHECK(GetProfileForTitle(game, &bound), "game mapping survives");
    CHECK(bound == 0, "mapping still points at the default, got %u", bound);

    /* Running it again must change nothing. */
    EnsureDefaultProfileIsStock();
    CHECK(GetProfileCount() == 1, "conversion is idempotent, got %u", GetProfileCount());
    CHECK(CurvesEqual(SectionOf(0, false).c_str(), CurvePreset_StockLike, false), "still stock-like");
}

static void TestDefaultIsEditable(void) {
    printf("Default is an ordinary editable profile\n");
    ResetConfig();
    EnsureProfilesInitialized();
    EnsureDefaultProfileIsStock();

    /* Edits to Default must survive every later boot. This is the whole point
     * of seeding once rather than restoring: nothing may reset it. */
    TemperaturePoint mine[3];
    mine[0].temperature_c = 30; mine[0].fanLevel_f = 0.10f;
    mine[1].temperature_c = 50; mine[1].fanLevel_f = 0.50f;
    mine[2].temperature_c = 70; mine[2].fanLevel_f = 0.90f;
    CHECK(SaveCurve(SectionOf(0, false).c_str(), mine, 3), "edit the default curve");

    EnsureDefaultProfileIsStock();
    EnsureDefaultProfileIsStock();

    TemperaturePoint got[MAX_TABLE_ENTRIES];
    const u32 n = LoadCurve(SectionOf(0, false).c_str(), got, MAX_TABLE_ENTRIES);
    CHECK(n == 3, "edited curve survives, got %u points", n);
    CHECK(got[1].fanLevel_f > 0.49f && got[1].fanLevel_f < 0.51f, "edited level survives, got %f", got[1].fanLevel_f);
    CHECK(!CurvesEqual(SectionOf(0, false).c_str(), CurvePreset_StockLike, false), "not reset back to stock");

    /* And it can be renamed and deleted like any other profile. */
    CHECK(SetProfileName(0, "My Default"), "default can be renamed");
    CHECK(NameOf(0) == "My Default", "rename took, got '%s'", NameOf(0).c_str());

    u32 other = 0;
    CHECK(CreateProfile("Other", 0, &other), "create a second profile");
    CHECK(DeleteProfile(0), "default can be deleted when it is not the last one");
    CHECK(!ProfileExists(0), "default is gone");
    CHECK(ProfileExists(other), "the other profile remains");
    CHECK(!DeleteProfile(other), "still cannot delete the last profile");
}

static void TestFanSensor(void) {
    printf("sensor selection\n");
    ResetConfig();
    EnsureProfilesInitialized();

    CHECK(GetFanSensor() == FanSensor_Soc, "defaults to SoC, got %u", GetFanSensor());

    CHECK(SetFanSensor(FanSensor_Skin), "select skin");
    CHECK(GetFanSensor() == FanSensor_Skin, "reads back skin, got %u", GetFanSensor());

    CHECK(SetFanSensor(FanSensor_Pcb), "select pcb");
    CHECK(GetFanSensor() == FanSensor_Pcb, "reads back pcb, got %u", GetFanSensor());

    CHECK(!SetFanSensor(FanSensor_Count), "rejects an out-of-range sensor");
    CHECK(GetFanSensor() == FanSensor_Pcb, "unchanged after bad set, got %u", GetFanSensor());

    /* A garbage value in the ini must not select a nonexistent sensor. */
    ini_putl(ConfigSection, KeyFanSensor, 99, FC_CONFIG_INI);
    CHECK(GetFanSensor() == FanSensor_Soc, "falls back to SoC on garbage, got %u", GetFanSensor());

    for (int i = 0; i < FanSensor_Count; ++i) {
        CHECK(GetSensorName((FanSensor)i)[0] != 0, "sensor %d has a name", i);
        CHECK(GetSensorDescription((FanSensor)i)[0] != 0, "sensor %d has a description", i);
        CHECK(SetFanSensor(i), "sensor %d is selectable", i);
        CHECK(GetFanSensor() == (u32)i, "sensor %d reads back", i);
    }

    /* Every sensor must have a distinct name, or the UI is ambiguous. */
    for (int i = 0; i < FanSensor_Count; ++i) {
        for (int j = i + 1; j < FanSensor_Count; ++j) {
            CHECK(strcmp(GetSensorName((FanSensor)i), GetSensorName((FanSensor)j)) != 0,
                  "sensors %d and %d have different names", i, j);
        }
    }
}

static void TestHorizonOcSensors(void) {
    printf("Horizon OC sensors degrade cleanly when absent\n");

    CHECK(SensorNeedsHorizonOc(FanSensor_Cpu), "CPU needs Horizon OC");
    CHECK(SensorNeedsHorizonOc(FanSensor_Gpu), "GPU needs Horizon OC");
    CHECK(SensorNeedsHorizonOc(FanSensor_Ram), "RAM needs Horizon OC");
    CHECK(!SensorNeedsHorizonOc(FanSensor_Soc), "SoC does not");
    CHECK(!SensorNeedsHorizonOc(FanSensor_Pcb), "PCB does not");
    CHECK(!SensorNeedsHorizonOc(FanSensor_Skin), "Skin does not");

    /* The harness reports Horizon OC as absent, so these must fail rather
     * than hand back a bogus reading. */
    CHECK(!IsHorizonOcAvailable(), "Horizon OC reported absent");

    for (int i = 0; i < FanSensor_Count; ++i) {
        if (!SensorNeedsHorizonOc((FanSensor)i)) {
            continue;
        }
        float value = 1234.0f;
        CHECK(!ReadSensor((FanSensor)i, &value), "sensor %d read fails without Horizon OC", i);
        CHECK(ReadSensorOrNegative((FanSensor)i) < 0.0f, "sensor %d reports -1 for the UI", i);
    }

    /* A NULL destination must never be written through. */
    CHECK(!ReadSensor(FanSensor_Cpu, NULL), "NULL out pointer is rejected");

    /* Selecting an unavailable sensor is allowed and persists; the sysmodule
     * falls back to SoC at read time rather than refusing the setting. */
    CHECK(SetFanSensor(FanSensor_Cpu), "CPU is still selectable without Horizon OC");
    CHECK(GetFanSensor() == FanSensor_Cpu, "selection persists, got %u", GetFanSensor());
}

static void TestDisplayRounding(void) {
    printf("display rounding\n");

    CHECK(RoundToInt(39.6f) == 40, "39.6 rounds up, got %d", RoundToInt(39.6f));
    CHECK(RoundToInt(39.4f) == 39, "39.4 rounds down, got %d", RoundToInt(39.4f));
    CHECK(RoundToInt(39.5f) == 40, "39.5 rounds up, got %d", RoundToInt(39.5f));
    CHECK(RoundToInt(0.0f) == 0, "zero stays zero, got %d", RoundToInt(0.0f));

    /* Interpolated levels are where rounding and truncation actually differ:
     * a new point between 40% and 50% can land on 0.4666. */
    CHECK(LevelToPercent(0.4666f) == 47, "0.4666 rounds to 47%%, got %d", LevelToPercent(0.4666f));
    CHECK((int)(0.4666f * 100) == 46, "truncation would show 46 (guards the premise)");
    CHECK(LevelToPercent(0.45f) == 45, "0.45 is 45%%, got %d", LevelToPercent(0.45f));

    /* Every level the UI can store must survive the round trip, because the
     * step index is derived from this value. */
    for (int pct = 0; pct <= 100; pct += 5) {
        const float level = (float)pct / 100.0f;
        CHECK(LevelToPercent(level) == pct, "%d%% round-trips, got %d", pct, LevelToPercent(level));
        CHECK(LevelToPercent(level) % 5 == 0, "%d%% lands on a step boundary", pct);
        CHECK(LevelToPercent(level) / 5 == pct / 5, "%d%% picks step %d, got %d", pct, pct / 5, LevelToPercent(level) / 5);
    }

    /* And the values written to the ini must match what was shown. */
    ResetConfig();
    EnsureProfilesInitialized();
    TemperaturePoint pts[3];
    pts[0].temperature_c = 30; pts[0].fanLevel_f = 0.45f;
    pts[1].temperature_c = 50; pts[1].fanLevel_f = 0.65f;
    pts[2].temperature_c = 70; pts[2].fanLevel_f = 0.85f;
    SaveCurve(SectionOf(0, false).c_str(), pts, 3);

    TemperaturePoint got[MAX_TABLE_ENTRIES];
    const u32 n = LoadCurve(SectionOf(0, false).c_str(), got, MAX_TABLE_ENTRIES);
    CHECK(n == 3, "curve saved, got %u", n);
    CHECK(LevelToPercent(got[0].fanLevel_f) == 45, "45%% survives the ini, got %d", LevelToPercent(got[0].fanLevel_f));
    CHECK(LevelToPercent(got[1].fanLevel_f) == 65, "65%% survives the ini, got %d", LevelToPercent(got[1].fanLevel_f));
    CHECK(LevelToPercent(got[2].fanLevel_f) == 85, "85%% survives the ini, got %d", LevelToPercent(got[2].fanLevel_f));
}

int main(void) {
    printf("=== NX-FanControl profile layer tests ===\n\n");

    TestDisplayRounding();
    TestSectionMapping();
    TestFreshInit();
    TestLegacyMigration();
    TestCreateAndSeed();
    TestRename();
    TestActiveProfile();
    TestDelete();
    TestStableIds();
    TestReorder();
    TestMaxProfiles();
    TestCorruptOrder();
    TestCurveRoundTrip();
    TestGameMappings();
    TestResolveAndStaleMappings();
    TestPresets();
    TestDefaultBecomesStock();
    TestDefaultIsEditable();
    TestFanSensor();
    TestHorizonOcSensors();

    printf("\n=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
