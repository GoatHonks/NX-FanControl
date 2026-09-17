#include "sensor_menu.hpp"
#include "settings.hpp"

namespace {
    /* "26C", or "N/A" for a source that needs Horizon OC and is not
     * answering, so an empty reading is never mistaken for a cold one. */
    std::string ReadingText(FanSensor sensor) {
        const float value = ReadSensorOrNegative(sensor);
        if (value >= 0.0f) {
            return std::to_string(RoundToInt(value)) + "C";
        }
        return SensorNeedsHorizonOc(sensor) ? "N/A" : "--";
    }

    std::string ItemValue(FanSensor sensor, bool active) {
        std::string text = ReadingText(sensor);
        if (active) {
            text += "   ●";
        }
        return text;
    }
}

std::string CurveSensorRowValue() {
    const FanSensor sensor = static_cast<FanSensor>(GetFanSensor());
    std::string label = GetSensorName(sensor);
    if (SensorNeedsHorizonOc(sensor) && ReadSensorOrNegative(sensor) < 0.0f) {
        label += "  (N/A)";
    }
    return label;
}

SensorMenu::SensorMenu() {
    this->_selected = GetFanSensor();
}

void SensorMenu::returnToSettings() {
    g_navJump = CurveSensorRowLabel;
    triggerExitFeedback();
    tsl::swapTo<Settings>();
}

tsl::elm::Element* SensorMenu::createUI() {
    auto frame = new tsl::elm::OverlayFrame("NX-FanControl", "Curve Sensor");

    auto list = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader("Drive the fan curve from", true));

    for (int i = 0; i < FanSensor_Count; ++i) {
        const FanSensor sensor = static_cast<FanSensor>(i);
        const bool active = ((u32)i == this->_selected);

        auto item = new tsl::elm::ListItem(GetSensorName(sensor), ItemValue(sensor, active));
        item->setClickListener([i](uint64_t keys) {
            if (!(keys & HidNpadButton_A)) {
                return false;
            }
            SetFanSensor((u32)i);
            SensorMenu::returnToSettings();
            return true;
        });

        this->_items[i] = item;
        list->addItem(item);
    }

    list->addItem(new tsl::elm::CategoryHeader("CPU / GPU / RAM need Horizon OC", true));

    /* Start on the source already in use. */
    list->jumpToItem(GetSensorName(static_cast<FanSensor>(this->_selected)));

    frame->setContent(list);

    return frame;
}

void SensorMenu::refreshValues() {
    for (int i = 0; i < FanSensor_Count; ++i) {
        if (this->_items[i] != nullptr) {
            const FanSensor sensor = static_cast<FanSensor>(i);
            this->_items[i]->setValue(ItemValue(sensor, (u32)i == this->_selected));
        }
    }
}

void SensorMenu::update() {
    static u64 counter = 0;
    counter++;

    /* Live readings make it obvious which source is worth picking. */
    if (counter % 6 == 0) {
        this->refreshValues();
    }
}

bool SensorMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) {
    if (keysDown & HidNpadButton_B) {
        SensorMenu::returnToSettings();
        return true;
    }
    return false;
}
