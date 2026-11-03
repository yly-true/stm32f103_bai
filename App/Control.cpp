/**
 * @file    Control.cpp
 * @brief   顶层控制类：采样 → 控制 → 输出 → 按键 → 界面
 *
 * 时间线：
 *   TIM1 每 5 ms 置 tickPending  →  main 循环调用 controlLoop()
 *   每 10 个节拍（50 ms）        →  OLED 刷新 + DataScope 上报
 * 按键全部走 5 ms 轮询（不再使用 EXTI），全系统只剩 TIM1 一个中断。
 */
#include "Control.hpp"

#include "Config.hpp"
#include "State.hpp"
#include "main.h"

namespace app {

// Meyer 单例：第一次调用时才构造，之后一直复用同一个对象。
// 因为所有成员都能常量初始化，编译器会把它放进 .data 而不是运行期构造。
Control& Control::instance()
{
    static Control control;
    return control;
}

// ------------------------------------------------------------- 中断入口 ----

void Control::onTickInterrupt()
{
    if (state_.tickPending) {
        ++state_.tickOverrun;   // 上一个节拍没处理完，说明主循环被拖住了
        return;
    }
    state_.tickPending = 1;
}

// ------------------------------------------------------------- 主循环体 ----

void Control::controlLoop()
{
    // 没有新节拍就直接返回，main 的循环会一直转
    if (!state_.tickPending) {
        return;
    }
    state_.tickPending = 0;

    scanKeys();             // ① 按键（5 ms 轮询 + 消抖）
    sample();               // ② 采样
    dispatch();             // ③ 手动平衡 / 自动起摆
    state_.voltage = board_.adc.batteryMilliVolt();

    // ④ 输出与指示
    if (state_.stopped) {
        board_.motor.stop();
    } else {
        board_.led.blink(100);
    }

    // ⑤ 用户键（PA2）的单击/双击/长按
    keys_.tick5ms(state_);
    keys_.updateIndicator(state_, board_);

    // ⑥ 每 50 ms 刷一次界面、上报一次数据
    if (++uiDivider_ >= cfg::kTelemetryPeriod) {
        uiDivider_ = 0;
        serviceUi();
    }
}

void Control::sample()
{
    state_.encoder = board_.motor.position();
    state_.angle = board_.adc.pendulumAngle();
    state_.angleRate = rateEstimator_.update(state_.angle);
}

void Control::dispatch()
{
    if (state_.autoRun) {
        if (!state_.stopped) {
            swingUp_.tick(state_, board_, balance_.anglePd(), balance_.positionPd());
        }
    } else {
        balance_.tick(state_, board_);
    }
}

void Control::serviceUi()
{
    DataScope();                                  // 上位机波形
    calibration_.tick(state_, keys_, board_);     // OLED 显示 / 校准向导
}

// ----------------------------------------------------------- 按键轮询 ----

void Control::scanKeys()
{
    // 1) 读当前电平，拼成与 GPIO 引脚号一致的位掩码（按键低电平有效）
    uint16_t raw = 0;
    if (KEY5  == 0) raw |= GPIO_PIN_5;
    if (KEY7  == 0) raw |= GPIO_PIN_7;
    if (KEY11 == 0) raw |= GPIO_PIN_11;
    if (KEY12 == 0) raw |= GPIO_PIN_12;

    // 2) 消抖：同一电平连续 kKeyDebounceTicks 拍才认账
    if (raw == rawKey_) {
        if (keyStableCount_ < cfg::kKeyDebounceTicks) {
            ++keyStableCount_;
            if (keyStableCount_ == cfg::kKeyDebounceTicks) {
                stableKey_ = raw;
            }
        }
    } else {
        rawKey_ = raw;
        keyStableCount_ = 0;
    }

    // 3) 只在"本拍新按下"时触发一次
    const uint16_t pressed = stableKey_ & ~prevKey_;
    prevKey_ = stableKey_;
    if (pressed != 0) {
        handleKey(pressed);
    }
}

void Control::handleKey(uint16_t pressed)
{
    if (pressed & GPIO_PIN_5) {             // 启停
        state_.stopped = !state_.stopped;
        if (!state_.stopped) {
            state_.swingUpFresh = false;    // 让平衡环重新取目标位置
        }
    } else if (pressed & GPIO_PIN_7) {      // 菜单 1→2→3→4→1
        state_.menu = (state_.menu >= 4) ? 1 : state_.menu + 1;
    } else if (pressed & GPIO_PIN_12) {     // PID 减
        switch (state_.menu) {
        case 1: state_.balanceKp  -= state_.amplitude[0]; break;
        case 2: state_.balanceKd  -= state_.amplitude[1]; break;
        case 3: state_.positionKp -= state_.amplitude[2]; break;
        case 4: state_.positionKd -= state_.amplitude[3]; break;
        default: break;
        }
    } else if (pressed & GPIO_PIN_11) {     // PID 加
        switch (state_.menu) {
        case 1: state_.balanceKp  += state_.amplitude[0]; break;
        case 2: state_.balanceKd  += state_.amplitude[1]; break;
        case 3: state_.positionKp += state_.amplitude[2]; break;
        case 4: state_.positionKd += state_.amplitude[3]; break;
        default: break;
        }
    }

    if (state_.balanceKp  < 0.0f) state_.balanceKp = 0.0f;
    if (state_.balanceKd  < 0.0f) state_.balanceKd = 0.0f;
    if (state_.positionKp < 0.0f) state_.positionKp = 0.0f;
    if (state_.positionKd < 0.0f) state_.positionKd = 0.0f;
}

} // namespace app

// ------------------------------------------------------- HAL 回调桥接 ----
extern "C" {

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim == &htim1) {
        app::Control::instance().onTickInterrupt();
    }
}

} // extern "C"
