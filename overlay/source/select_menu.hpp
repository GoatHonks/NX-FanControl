#pragma once

#include <tesla.hpp>
#include <fancontrol.hpp>
#include "curve.hpp"
#include "utils.hpp"

class SelectMenu : public tsl::Gui {
private:
    u32 _i = 0;

    tsl::elm::CategoryHeader* _tempLabel;
    tsl::elm::CategoryHeader* _fanLabel;

    void exitPoint();

public:
    SelectMenu(u32 i);

    virtual tsl::elm::Element* createUI() override;

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override;
};
