#include <fancontrol.hpp>
#include "select_menu.hpp"
#include "curve_menu.hpp"
#include "utils.hpp"
#include "hold_item.hpp"

#include <algorithm>
#include <functional>
#include <vector>

namespace {

    constexpr int MinTemp = 20;
    constexpr int MaxTemp = 80;

}

SelectMenu::SelectMenu(u32 i) {
    this->_i = i;
    this->_tempLabel = new tsl::elm::CategoryHeader(std::to_string(g_editCurve->points[i].temperature_c) + "C", true);
    this->_fanLabel = new tsl::elm::CategoryHeader(std::to_string(LevelToPercent(g_editCurve->points[i].fanLevel_f)) + "%", true);
}

void SelectMenu::exitPoint() {
    std::string label = FormatPointLabel(g_editCurve->points[this->_i]);
    g_editCurve->persist();
    g_navJump = label;
    triggerExitFeedback();
    tsl::swapTo<CurveMenu>(SwapDepth(2));
}

tsl::elm::Element* SelectMenu::createUI() {
    auto frame = new tsl::elm::OverlayFrame("NX-FanControl", "Curve Point");

    auto list = new tsl::elm::List();

    const int curTemp = g_editCurve->points[this->_i].temperature_c;

    std::vector<int> freeTemps;
    for (int t = MinTemp; t <= MaxTemp; t += 5) {
        if (!g_editCurve->tempTaken(t, this->_i)) {
            freeTemps.push_back(t);
        }
    }

    list->addItem(this->_tempLabel);
    if (freeTemps.size() >= 2) {
        int startIdx = 0;
        for (size_t k = 0; k < freeTemps.size(); k++) {
            if (freeTemps[k] == curTemp) {
                startIdx = (int)k;
            }
        }

        auto stepTemp = new tsl::elm::StepTrackBar("C", freeTemps.size());
        stepTemp->setValueChangedListener([this, freeTemps](u16 value) {
            if (value >= freeTemps.size()) {
                return;
            }
            int newTemp = freeTemps[value];
            if (g_editCurve->trySetTemp(this->_i, newTemp)) {
                this->_tempLabel->setText(std::to_string(newTemp) + "C");
            }
        });
        stepTemp->setProgress(startIdx);
        list->addItem(stepTemp);
    }

    list->addItem(this->_fanLabel);
    auto stepFanL = new tsl::elm::StepTrackBar("%", 21);
    stepFanL->setValueChangedListener([this](u16 value) {
        this->_fanLabel->setText(std::to_string(value * 5) + "%");
        g_editCurve->setLevel(this->_i, (float)(value * 5) / 100.0f);
    });
    stepFanL->setProgress(LevelToPercent(g_editCurve->points[this->_i].fanLevel_f) / 5);
    list->addItem(stepFanL);

    list->addItem(new HoldToConfirmItem("Delete Point", "can't delete (min. points)", g_editCurve->count > 2, [this]() {
        std::string neighbour = FormatPointLabel(g_editCurve->points[this->_i == 0 ? 1 : this->_i - 1]);
        if (g_editCurve->removePoint(this->_i)) {
            g_navJump = neighbour;
            triggerExitFeedback();
            tsl::swapTo<CurveMenu>(SwapDepth(2));
        }
    }));

    frame->setContent(list);

    return frame;
}

bool SelectMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
    if (keysDown & HidNpadButton_B) {
        this->exitPoint();
        return true;
    }
    return false;
}
