#include "profile_menu.hpp"
#include "main_menu.hpp"
#include "hold_item.hpp"

#include <string>

namespace {
    const std::string AddProfileLabel = "+  Add Profile";

    std::string ProfileNameOf(u32 id) {
        char name[MaxProfileNameLength + 1];
        GetProfileName(id, name, sizeof(name));
        return std::string(name);
    }
}

std::string ActiveProfileName() {
    return ProfileNameOf(GetActiveProfileId());
}

ProfileMenu::ProfileMenu() {
    EnsureProfilesInitialized();
    this->_count  = GetProfileIds(this->_ids, MaxProfiles);
    this->_active = GetActiveProfileId();
}

tsl::elm::Element* ProfileMenu::createUI() {
    auto frame = new tsl::elm::OverlayFrame("NX-FanControl", "Profiles");

    auto list = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader("Select Profile", true));

    for (u32 i = 0; i < this->_count; ++i) {
        const u32 id = this->_ids[i];
        const bool isActive = (id == this->_active);
        const std::string name = ProfileNameOf(id);

        auto item = new tsl::elm::ListItem(name, isActive ? "Active" : "");
        item->setClickListener([this, id, name](uint64_t keys) {
            if (!(keys & HidNpadButton_A)) {
                return false;
            }
            if (id == this->_active) {
                return true;
            }
            if (SetActiveProfileId(id)) {
                /* Repoint the editor at the newly active profile so the main
                 * menu's graph and curve editors follow the switch. */
                BindCurvesToProfile(id);
                g_editCurve = &g_curve;
                g_navJump = name;
                tsl::swapTo<ProfileMenu>();
            }
            return true;
        });
        list->addItem(item);
    }

    list->addItem(new tsl::elm::CategoryHeader("Manage", true));

    const bool canAdd = this->_count < MaxProfiles;
    auto addItem = new tsl::elm::ListItem(AddProfileLabel, canAdd ? "" : "max reached");
    addItem->setClickListener([this, canAdd](uint64_t keys) {
        if (!(keys & HidNpadButton_A)) {
            return false;
        }
        if (!canAdd) {
            return true;
        }
        /* Seeded from the active profile, so a new entry starts as a copy the
         * user can tweak rather than an empty curve. */
        u32 newId = 0;
        if (CreateProfile(NULL, this->_active, &newId)) {
            g_navJump = ProfileNameOf(newId);
            tsl::swapTo<ProfileMenu>();
        }
        return true;
    });
    list->addItem(addItem);

    const bool canDelete = this->_count > 1;
    list->addItem(new HoldToConfirmItem("Delete Active Profile", "can't delete (last one)", canDelete, [this]() {
        if (DeleteProfile(this->_active)) {
            BindCurvesToActiveProfile();
            g_editCurve = &g_curve;
            g_navJump = ActiveProfileName();
            triggerExitFeedback();
            tsl::swapTo<ProfileMenu>();
        }
    }));

    /* Per-game binding. The overlay is the only place this can be captured,
     * because it is the only part of the tool that runs while a game is in
     * the foreground. */
    list->addItem(new tsl::elm::CategoryHeader("Current Game", true));

    auto gameToggle = new tsl::elm::ToggleListItem("Per-Game Profiles", IsGameProfilesEnabled());
    gameToggle->setStateChangedListener([](bool state) {
        SetGameProfilesEnabled(state);
    });
    list->addItem(gameToggle);

    const u64 titleId = GetRunningTitleId();
    if (titleId == 0) {
        list->addItem(new tsl::elm::ListItem("No game running", "start a game first"));
    } else {
        char titleText[24];
        snprintf(titleText, sizeof(titleText), "%016lX", titleId);

        u32 boundProfile = 0;
        const bool bound = GetProfileForTitle(titleId, &boundProfile);

        list->addItem(new tsl::elm::ListItem("Title ID", titleText));

        const std::string assignLabel = "Assign to " + ProfileNameOf(this->_active);
        auto assignItem = new tsl::elm::ListItem(assignLabel, bound ? ProfileNameOf(boundProfile) : "not assigned");
        assignItem->setClickListener([this, titleId](uint64_t keys) {
            if (!(keys & HidNpadButton_A)) {
                return false;
            }
            if (SetProfileForTitle(titleId, this->_active)) {
                g_navJump = "Title ID";
                tsl::swapTo<ProfileMenu>();
            }
            return true;
        });
        list->addItem(assignItem);

        if (bound) {
            auto clearItem = new tsl::elm::ListItem("Clear Assignment");
            clearItem->setClickListener([titleId](uint64_t keys) {
                if (!(keys & HidNpadButton_A)) {
                    return false;
                }
                if (ClearProfileForTitle(titleId)) {
                    g_navJump = "Title ID";
                    tsl::swapTo<ProfileMenu>();
                }
                return true;
            });
            list->addItem(clearItem);
        }
    }

    list->addItem(new tsl::elm::CategoryHeader("Rename in the Manager app", true));

    if (!g_navJump.empty()) {
        list->jumpToItem(g_navJump);
        g_navJump.clear();
    }

    frame->setContent(list);

    return frame;
}

bool ProfileMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
    if (keysDown & HidNpadButton_B) {
        g_navJump = "Profile";
        triggerExitFeedback();
        tsl::swapTo<MainMenu>();
        return true;
    }
    return false;
}
