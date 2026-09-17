#pragma once

#include <tesla.hpp>
#include <fancontrol.hpp>
#include "curve.hpp"
#include "utils.hpp"

/* Profile list: pick the active profile, add one, or remove one.
 *
 * Renaming lives in the manager NRO rather than here, because a Tesla
 * overlay cannot launch the system keyboard (swkbd needs a library applet,
 * which the overlay loader has no way to start). */
class ProfileMenu : public tsl::Gui {
public:
    ProfileMenu();

    virtual tsl::elm::Element* createUI() override;

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override;

private:
    u32 _ids[MaxProfiles];
    u32 _count  = 0;
    u32 _active = 0;
};

/* "Profiles" row label, showing the active profile as its value. */
std::string ActiveProfileName();
