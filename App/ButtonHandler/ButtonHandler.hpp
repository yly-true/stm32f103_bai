#ifndef APP_BUTTON_HANDLER_HPP
#define APP_BUTTON_HANDLER_HPP

#include "Control/ControlState.hpp"

namespace app {

// PA5 启动、PA2 软件急停，均为低电平有效。
class ButtonHandler {
public:
    // 每轮主循环检查，不等待控制节拍或按键消抖；急停优先于启动。
    bool checkEmergencyStop(ControlState& state);

    // 每个控制节拍调用：启动键消抖，松开后才允许下一次启动。
    void scan(ControlState& state);

private:
    bool startArmed_ = false;  // 上电或急停后，必须先松开启动键
    uint8_t pressTicks_ = 0;
    uint8_t releaseTicks_ = 0;
};

} // namespace app

#endif
