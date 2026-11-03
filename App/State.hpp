#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <stdint.h>
#include "Config.hpp"

namespace app {

// 整个系统共享的运行状态。
struct State {
    // ---- 实时测量值 ----
    int   encoder      = 0;                 // 滑块位置（编码器脉冲）
    int   voltage      = 0;                 // 电池电压，mV
    float angle        = 0.0f;              // 摆杆角度，ADC 原始值
    float angleRate    = 0.0f;              // 摆杆角速度，差分×滤波

    // ---- 控制量 ----
    int   motorPwm     = 0;                 // 最终输出（限幅前）
    int   balancePwm   = 0;                 // 角度环输出
    int   positionPwm  = 0;                 // 位置环输出
    int   positionZero = cfg::kEncoderPowerOn; // 位置环目标值
    int   divider      = 0;                 // 位置环分频计数

    // ---- 模式与保护 ----
    bool  stopped      = true;              // 上电默认停机，等用户按键
    bool  autoRun      = false;             // true: 自动起摆；false: 手动平衡
    bool  swingUpFresh = true;              // 为 false 时平衡环重新取目标位置

    // ---- 可在线调整的 PID 系数 ----
    float balanceKp = cfg::kBalanceKpDefault;
    float balanceKd = cfg::kBalanceKdDefault;
    float positionKp = cfg::kPositionKpDefault;
    float positionKd = cfg::kPositionKdDefault;

    // ---- 菜单与步进量（按键调参用）----
    int   menu = 1;
    int   amplitude[4] = {5, 20, 1, 10};

    // ---- 中断 → 主循环的信号 ----
    volatile uint8_t  tickPending = 0;      // TIM1 每 5 ms 置 1
    volatile uint32_t tickOverrun = 0;      // 主循环来不及处理时丢掉的节拍数

    // ---- UI 状态 ----
    bool  calibrationRequested = false;
    bool  restoreAfterCalibration = false;
};

} // namespace app

#endif
