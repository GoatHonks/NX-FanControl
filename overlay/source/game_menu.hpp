#pragma once

#include <tesla.hpp>
#include <fancontrol.hpp>
#include "curve.hpp"
#include "utils.hpp"

#include <string>

constexpr const char* GameProfilesRowLabel = "Per-Game Profiles";

/* The per-game screen, opened from the main page: the feature's on/off
 * switch and the running game's assignment. It lives in the overlay because
 * the overlay is the only part of the tool that runs while a game is in the
 * foreground. */
class GameProfilesMenu : public tsl::Gui {
public:
    GameProfilesMenu() = default;

    virtual tsl::elm::Element* createUI() override;
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override;
};

/* Picks which profile the running game should use.
 *
 * Opened from the game row on the per-game screen, and only when there is
 * more than one profile to choose between; with a single profile the row
 * assigns it directly. */
class AssignGameMenu : public tsl::Gui {
public:
    AssignGameMenu(u64 titleId, const std::string &gameLabel);

    virtual tsl::elm::Element* createUI() override;
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override;

private:
    u64 _titleId;
    std::string _gameLabel;

    void returnToMain();
};

/* The main screen's game row: the running game's name (or its title id if the
 * name can't be read), and which profile it is bound to. Empty title id means
 * no game is running. */
struct RunningGame {
    u64 titleId = 0;
    std::string label;
};

RunningGame QueryRunningGame();

/* Binds a game to a profile, switching per-game profiles on if needed: an
 * assignment does nothing while the feature is off, and silently doing
 * nothing is how this looked broken. */
bool AssignGameToProfile(u64 titleId, u32 profileId);
