#include "Keys.hpp"

#include "Config.hpp"
#include "State.hpp"
#include "main.h"   // 引脚宏：KEY2 / KEY5 / KEY7 / KEY11 / KEY12

namespace app {

namespace {
// 按键都是低电平有效
inline bool userKey()  { return KEY2  == 0; }
inline bool startKey() { return KEY5  == 0; }
inline bool menuKey()  { return KEY7  == 0; }
inline bool plusKey()  { return KEY11 == 0; }
inline bool minusKey() { return KEY12 == 0; }
} // namespace

// ------------------------------------------------------------ 单击/双击 ----
// 原 click_N_Double()：单击返回 1，双击返回 2，无用动作返回 0。
// window 以 5 ms 控制周期为单位，100 即 500 ms。
int Keys::clickSingleOrDouble(int window)
{
    if (userKey()) ++forever_;
    else           forever_ = 0;

    if (!userKey() && !armed_) {
        armed_ = true;
    }

    if (!pressed_) {
        if (armed_) {
            ++pending_;
            pressed_ = true;
        }
        if (pending_ == 2) {
            pending_ = 0;
            single_ = 0;
            return 2;
        }
    }

    if (!userKey()) {   // 松开
        armed_ = false;
        pressed_ = false;
    }

    if (pending_ == 1) {
        ++single_;
        if (single_ > window && forever_ < window) {
            pending_ = 0;
            single_ = 0;
            return 1;
        }
        if (forever_ > window) {
            pending_ = 0;
            single_ = 0;
        }
    }
    return 0;
}

// ------------------------------------------------------------- 5 ms 扫描 ----
void Keys::tick5ms(State& s)
{
// 单击/双击：每次 ±4 个脉冲，累计满 1040（电机一圈）才算完成一次调整
    const int action = clickSingleOrDouble(cfg::kDoubleClickTicks);
    if (action == 1) stepFlag_ = 1;
    if (action == 2) stepFlag_ = 2;

    if (stepFlag_ == 1) {
        s.positionZero += 4;
        stepCount_ += 4;
        if (stepCount_ == 1040) { stepFlag_ = 0; stepCount_ = 0; }
    }
    if (stepFlag_ == 2) {
        s.positionZero -= 4;
        stepCount_ += 4;
        if (stepCount_ == 1040) { stepFlag_ = 0; stepCount_ = 0; }
    }

    // 长按用户键 1 s：位置归零 + 切换手动/自动起摆（松手后才允许再次触发）
    if (userKey()) {
        if (holdUserArmed_ && ++holdUser_ > cfg::kLongPressTicks) {
            holdUser_ = 0;
            holdUserArmed_ = false;
            s.positionZero = cfg::kEncoderPowerOn;
            s.autoRun = !s.autoRun;
        }
    } else {
        holdUser_ = 0;
        holdUserArmed_ = true;
    }
}

// 停机时用 LED 指示当前模式：自动起摆常亮，手动常灭
void Keys::updateIndicator(const State& s, Board& io)
{
    if (!s.stopped) return;
    if (s.autoRun) io.led.on();
    else           io.led.off();
}

// --------------------------------------------------------- 校准向导用接口 ----
int Keys::click()
{
    const bool anyDown = startKey() || plusKey() || minusKey();
    if (clickHeld_ && anyDown) {
        clickHeld_ = false;
        HAL_Delay(50);
        if (startKey()) return 5;
        if (plusKey())  return 11;
        if (minusKey()) return 12;
        return 0;
    }
    if (!anyDown) clickHeld_ = true;
    return 0;
}

bool Keys::longPressMenu()
{
    if (!holdMenuFired_ && menuKey()) {
        ++holdMenu_;
    } else {
        holdMenu_ = 0;
    }
    if (holdMenu_ > 20) {
        holdMenu_ = 0;
        holdMenuFired_ = true;
        return true;
    }
    if (holdMenuFired_) holdMenuFired_ = false;
    return false;
}

} // namespace app
