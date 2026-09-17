#pragma once

#include <fancontrol.hpp>
#include <string>

class CurveStore {
public:
    TemperaturePoint points[MAX_TABLE_ENTRIES];
    u32 count = 0;

    CurveStore() = default;
    explicit CurveStore(bool docked) : _docked(docked) {}

    /* Points this store at a profile's curve. Does not load; call
     * loadOrDefault() afterwards. */
    void bindToProfile(u32 id);

    bool isDockedProfile() const { return this->_docked; }
    const char* sectionName() const { return this->_section.c_str(); }

    void loadOrDefault();
    bool persist();
    void sortByTemp();

    bool addPoint();
    bool removePoint(u32 index);

    bool trySetTemp(u32 index, int temperature_c);
    void setLevel(u32 index, float level);

    bool tempTaken(int temperature_c, u32 exceptIndex) const;

private:
    bool _docked = false;
    u32 _profileId = 0;
    std::string _section = CurveSection;
};

extern CurveStore g_curve;
extern CurveStore g_dockedCurve;
extern CurveStore* g_editCurve;
extern std::string g_navJump;

/* Rebinds both curve stores to the given profile and loads them. */
void BindCurvesToProfile(u32 id);

/* Rebinds to whichever profile is currently active in the config. */
void BindCurvesToActiveProfile();

std::string FormatPointLabel(const TemperaturePoint& point);

std::string HandheldCurveButtonLabel();
constexpr const char* DockedCurveButtonLabel = "Edit Docked Curve";
