#pragma once

#include <tesla.hpp>
#include <fancontrol.hpp>
#include "curve.hpp"
#include "utils.hpp"

/* Picks which temperature source drives the fan curve.
 *
 * A list rather than a cycling value: with six sources, stepping through them
 * one at a time to reach the one you want is tedious, and each step would
 * write the config. */
class SensorMenu : public tsl::Gui {
public:
    SensorMenu();

    virtual tsl::elm::Element* createUI() override;
    virtual void update() override;
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override;

private:
    tsl::elm::ListItem* _items[FanSensor_Count] = {};
    u32 _selected = 0;

    void refreshValues();
    static void returnToSettings();
};

/* Label shown on the Settings row that opens this menu. */
std::string CurveSensorRowValue();

constexpr const char* CurveSensorRowLabel = "Curve Sensor";
