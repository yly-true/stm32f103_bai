#include "ButtonHandler.hpp"

#include "Control/Config.hpp"
#include "main.h"

namespace app {

bool ButtonHandler::checkEmergencyStop(ControlState& state)
{
    if (KEY2 != 0) return false;

    state.mode = RunMode::kStopped;
    state.motorPwm = state.balancePwm = state.positionPwm = 0;
    state.divider = 0;
    startArmed_ = false;
    pressTicks_ = releaseTicks_ = 0;
    return true;
}

void ButtonHandler::scan(ControlState& state)
{
    if (checkEmergencyStop(state)) return;

    if (KEY5 != 0) {
        pressTicks_ = 0;
        if (releaseTicks_ < cfg::kKeyDebounceTicks) ++releaseTicks_;
        if (releaseTicks_ >= cfg::kKeyDebounceTicks) startArmed_ = true;
        return;
    }

    releaseTicks_ = 0;
    if (!startArmed_) return;
    if (++pressTicks_ < cfg::kKeyDebounceTicks) return;

    startArmed_ = false;
    pressTicks_ = 0;
    if (state.mode == RunMode::kStopped) {
        // 只"解锁"，真正开始 PID 要等摆杆进入竖直窗口。
        state.mode = RunMode::kArmed;
        state.motorPwm = state.balancePwm = state.positionPwm = 0;
        state.divider = 0;
    }
}

} // namespace app
