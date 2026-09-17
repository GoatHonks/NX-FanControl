#pragma once

#include <tesla.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace hold_item {
    constexpr tsl::Color DeleteFill(0xF, 0x3, 0x3, 0xF);
    constexpr tsl::Color AButtonColor(0x3, 0xA, 0xF, 0xF);
    constexpr u64 HoldDurationNs = 500000000ULL;
}

/* A list item that must be held rather than tapped, with a fill bar showing
 * progress. Used for destructive actions so they cannot be triggered by a
 * stray press. */
class HoldToConfirmItem : public tsl::elm::ListItem {
public:
    HoldToConfirmItem(const std::string& label, const std::string& disabledReason, bool enabled, std::function<void()> onFilled)
        : tsl::elm::ListItem(label, enabled ? "" : disabledReason), _enabled(enabled), _onFilled(onFilled) {
        this->m_flags.m_useClickAnimation = false;
    }

    virtual void drawValue(tsl::gfx::Renderer* renderer, s32 yOffset, bool useClickTextColor) override {
        if (!this->_enabled) {
            tsl::elm::ListItem::drawValue(renderer, yOffset, useClickTextColor);
            return;
        }
        const s32 x = this->getX() + this->m_maxWidth + 47;
        const s32 y = renderer->getVerticalCenterBaseline(this->getY(), this->m_listItemHeight, 20);
        const tsl::Color base = [&] {
            if (useClickTextColor) {
                return tsl::clickTextColor;
            }
            if (this->m_focused && ult::useSelectionValue) {
                return tsl::selectedValueTextColor;
            }
            return tsl::onTextColor;
        }();
        static const std::vector<std::string> special = {""};
        renderer->drawStringWithColoredSections(this->m_value, false, special, x, y, 20, base, hold_item::AButtonColor);
    }

    virtual void draw(tsl::gfx::Renderer* renderer) override {
        if (this->_progress > 0.0f) {
            const s32 barWidth = (s32)((this->getWidth() - 8) * this->_progress);
            renderer->drawRect(this->getX() + 4, this->getY() + 1, barWidth, this->getHeight() - 2, renderer->a(hold_item::DeleteFill));
        }
        tsl::elm::ListItem::draw(renderer);
    }

    virtual bool onClick(u64 keys) override {
        return false;
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
        if (this->_enabled && (keysHeld & HidNpadButton_A) && !(keysHeld & ~HidNpadButton_A & ALL_KEYS_MASK)) {
            if (this->_holdStart == 0) {
                this->_holdStart = armGetSystemTick();
            }
            const u64 elapsed = armTicksToNs(armGetSystemTick() - this->_holdStart);
            this->_progress = std::min(1.0f, (float)elapsed / (float)hold_item::HoldDurationNs);
            if (this->_progress >= 1.0f) {
                this->_holdStart = 0;
                this->_progress = 0.0f;
                this->_onFilled();
            }
            return true;
        }
        this->_holdStart = 0;
        this->_progress = 0.0f;
        return false;
    }

private:
    bool _enabled;
    std::function<void()> _onFilled;
    u64 _holdStart = 0;
    float _progress = 0.0f;
};
