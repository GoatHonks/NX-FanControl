#include "game_menu.hpp"
#include "main_menu.hpp"

namespace {
    std::string ProfileNameOf(u32 id) {
        char name[MaxProfileNameLength + 1];
        GetProfileName(id, name, sizeof(name));
        return std::string(name);
    }
}

RunningGame QueryRunningGame() {
    RunningGame game;
    game.titleId = GetRunningTitleId();
    if (game.titleId == 0) {
        return game;
    }

    char name[0x200];
    if (GetTitleName(game.titleId, name, sizeof(name))) {
        game.label = name;
    } else {
        char id[24];
        FormatTitleId(game.titleId, id, sizeof(id));
        game.label = id;
    }
    return game;
}

bool AssignGameToProfile(u64 titleId, u32 profileId) {
    if (!SetProfileForTitle(titleId, profileId)) {
        return false;
    }
    if (!IsGameProfilesEnabled()) {
        SetGameProfilesEnabled(true);
    }
    return true;
}

/* ---- per-game screen ---- */

tsl::elm::Element* GameProfilesMenu::createUI() {
    auto frame = new tsl::elm::OverlayFrame("NX-FanControl", GameProfilesRowLabel);

    auto list = new tsl::elm::List();

    auto toggle = new tsl::elm::ToggleListItem(GameProfilesRowLabel, IsGameProfilesEnabled());
    toggle->setStateChangedListener([](bool state) {
        SetGameProfilesEnabled(state);
    });
    list->addItem(toggle);

    list->addItem(new tsl::elm::CategoryHeader("Running Game", true));

    const RunningGame game = QueryRunningGame();
    if (game.titleId == 0) {
        list->addItem(new tsl::elm::ListItem("No game running", "start a game first"));
    } else {
        u32 boundProfile = 0;
        const bool bound = GetProfileForTitle(game.titleId, &boundProfile);

        char boundName[MaxProfileNameLength + 1] = "not assigned";
        if (bound) {
            GetProfileName(boundProfile, boundName, sizeof(boundName));
        }

        auto gameItem = new tsl::elm::ListItem(game.label, boundName);
        gameItem->setClickListener([game](uint64_t keys) {
            if (!(keys & HidNpadButton_A)) {
                return false;
            }

            u32 ids[MaxProfiles];
            const u32 count = GetProfileIds(ids, MaxProfiles);

            /* With one profile there is nothing to choose: assign it and
             * refresh this screen. With more, open the list. */
            if (count == 1) {
                AssignGameToProfile(game.titleId, ids[0]);
                g_navJump = game.label;
                tsl::swapTo<GameProfilesMenu>();
            } else if (count > 1) {
                g_navJump.clear();
                tsl::swapTo<AssignGameMenu>(game.titleId, game.label);
            }
            return true;
        });
        list->addItem(gameItem);
    }

    if (!g_navJump.empty()) {
        list->jumpToItem(g_navJump);
        g_navJump.clear();
    }

    frame->setContent(list);

    return frame;
}

bool GameProfilesMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
    if (keysDown & HidNpadButton_B) {
        g_navJump = GameProfilesRowLabel;
        triggerExitFeedback();
        tsl::swapTo<MainMenu>();
        return true;
    }
    return false;
}

/* ---- profile picker ---- */

AssignGameMenu::AssignGameMenu(u64 titleId, const std::string &gameLabel)
    : _titleId(titleId), _gameLabel(gameLabel) {}

void AssignGameMenu::returnToMain() {
    /* Back to the per-game screen, landing on the game that was assigned. */
    g_navJump = this->_gameLabel;
    triggerExitFeedback();
    tsl::swapTo<GameProfilesMenu>();
}

tsl::elm::Element* AssignGameMenu::createUI() {
    auto frame = new tsl::elm::OverlayFrame("NX-FanControl", "Assign Game");

    auto list = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader(this->_gameLabel, true));

    u32 bound = 0;
    const bool isBound = GetProfileForTitle(this->_titleId, &bound);

    u32 ids[MaxProfiles];
    const u32 count = GetProfileIds(ids, MaxProfiles);

    std::string startOn;
    for (u32 i = 0; i < count; ++i) {
        const u32 id = ids[i];
        const bool current = isBound && id == bound;
        const std::string name = ProfileNameOf(id);
        if (current) {
            startOn = name;
        }

        auto item = new tsl::elm::ListItem(name, current ? "●" : "");
        item->setClickListener([this, id](uint64_t keys) {
            if (!(keys & HidNpadButton_A)) {
                return false;
            }
            AssignGameToProfile(this->_titleId, id);
            this->returnToMain();
            return true;
        });
        list->addItem(item);
    }

    if (isBound) {
        list->addItem(new tsl::elm::CategoryHeader("Assigned", true));
        auto clearItem = new tsl::elm::ListItem("Clear Assignment");
        clearItem->setClickListener([this](uint64_t keys) {
            if (!(keys & HidNpadButton_A)) {
                return false;
            }
            ClearProfileForTitle(this->_titleId);
            this->returnToMain();
            return true;
        });
        list->addItem(clearItem);
    }

    /* Start on the profile this game already uses. */
    if (!startOn.empty()) {
        list->jumpToItem(startOn);
    }

    frame->setContent(list);

    return frame;
}

bool AssignGameMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
    if (keysDown & HidNpadButton_B) {
        this->returnToMain();
        return true;
    }
    return false;
}
