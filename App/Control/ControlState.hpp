#ifndef APP_CONTROL_STATE_HPP
#define APP_CONTROL_STATE_HPP

#include <stdint.h>
#include "Config.hpp"

namespace app {

// 运行模式：只有 kBalancing 会输出 PWM，另外两种模式电机都自由滑行。
//   kStopped ──按启动键──▶ kArmed ──摆进竖直窗口(±300)──▶ kBalancing
//       ▲                                                   │
//       └──────急停 / 低压 / 掉出窗口(±600)，需重新按键──────┘
enum class RunMode : uint8_t {
    kStopped   = 0,  // 上电默认 / 急停 / 低压：不输出，等用户按键
    kArmed     = 1,  // 已按启动键，等摆杆进入竖直窗口；电机不输出，可手动扶摆
    kBalancing = 2,  // 摆杆在竖直窗口内：角度环 + 位置环 PID 输出
};

// 整个系统共享的运行状态。
struct ControlState {
    // ---- 实时测量值 ----
    int   encoder      = 0;                 // 滑块位置（编码器脉冲）
    int   voltage      = 0;                 // 电池电压，mV
    float angle        = 0.0f;              // 相对竖直向上的角度，度（0 = 竖直）
    float angleRate    = 0.0f;              // 角速度，度/秒

    // ---- 控制量 ----
    int   motorPwm     = 0;                 // 最终输出（限幅前）
    int   balancePwm   = 0;                 // 角度环输出
    int   positionPwm  = 0;                 // 位置环输出
    int   positionZero = cfg::kEncoderPowerOn; // 位置环目标值，进入平衡时取当前位置
    int   divider      = 0;                 // 位置环分频计数

    // ---- 模式 ----
    RunMode mode = RunMode::kStopped;

    // ---- 中断 → 主循环的信号 ----
    volatile uint8_t  tickPending = 0;      // TIM1 每 5 ms 置 1
};

} // namespace app

#endif
