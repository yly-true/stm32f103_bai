/**
 * @file    Control.cpp
 * @brief   顶层控制类：采样 → 控制 → 输出 → 按键
 *
 * 时间线：
 *   TIM1 每 5 ms 置 tickPending  →  main 循环调用 controlLoop()
 * 启动键每 5 ms 消抖，软件急停每轮主循环检查；USART1 DMA 发送日志（50 ms 一次）。
 *
 * 运行模式（ControlState.hpp 的 RunMode）：
 *   kStopped   上电/急停/低压，电机不输出
 *   kArmed     已按启动键，等摆杆进入竖直窗口，电机不输出
 *   kBalancing 竖直窗口内，PID 输出
 * 串口每 50 ms 上报一次当前模式：0=停机，1=等竖直，2=平衡中。
 */
#include "Control.hpp"

#include <stdarg.h>
#include <stdio.h>

#include "Config.hpp"
#include "ControlState.hpp"
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
    state_.tickPending = 1;
}

int Control::uart1Printf(const char* format, ...)
{
    if (!format || __get_IPSR() != 0U ||
        huart1.gState != HAL_UART_STATE_READY || huart1.hdmatx == nullptr ||
        HAL_DMA_GetState(huart1.hdmatx) != HAL_DMA_STATE_READY) {
        return -1;
    }

    va_list args;
    va_start(args, format);
    const int length = vsnprintf(uart1TxBuffer_, sizeof(uart1TxBuffer_), format, args);
    va_end(args);

    if (length < 0 || length >= static_cast<int>(sizeof(uart1TxBuffer_))) return -1;
    if (length == 0) return 0;

    return HAL_UART_Transmit_DMA(&huart1, reinterpret_cast<uint8_t*>(uart1TxBuffer_),
                                static_cast<uint16_t>(length)) == HAL_OK
               ? length : -1;
}

// ------------------------------------------------------------- 主循环体 ----

void Control::controlLoop()
{
    auto& control = Control::instance();

    // 软件急停每轮检查，尽早清零 PWM，不等待 5 ms 控制节拍。
    if (control.buttons_.checkEmergencyStop(control.state_)) {
        control.hardware_.motor.stop();
        control.hardware_.led.off();
    }

    // 没有新节拍就直接返回，main 的循环会一直转
    if (!control.state_.tickPending) {
        return;
    }
    control.state_.tickPending = 0;

    control.buttons_.scan(control.state_);  // ① 启动键消抖，急停优先
    control.sample();                      // ② 采样

    // ADC 采样期间也可能按下急停，输出前再确认一次。
    if (control.buttons_.checkEmergencyStop(control.state_)) {
        control.hardware_.motor.stop();
        control.hardware_.led.off();
        return;
    }
    control.pendulum_.tick(control.state_, control.hardware_);  // ③ 按模式驱动电机

    // ④ 输出与指示：电机由 PendulumController 按模式驱动，这里只管串口和 LED
    const RunMode mode = control.state_.mode;
    switch (mode) {
    case RunMode::kStopped:
        control.hardware_.led.off();
        break;
    case RunMode::kArmed:          // 慢闪（2 s）：等摆杆竖直
        control.hardware_.led.blink(400);
        break;
    case RunMode::kBalancing:      // 快闪（0.5 s）：PID 平衡中
        control.hardware_.led.blink(100);
        break;
    }

    // 串口按 50 ms 上报，避免 5 ms 节拍把 DMA 占满
    if (++control.telemetryDivider_ >= cfg::kTelemetryDivider) {
        control.telemetryDivider_ = 0;
        control.uart1Printf("%d\r\n", static_cast<int>(mode));
    }
}

void Control::sample()
{
    state_.encoder = hardware_.motor.position();

    // ADC 原始值 → 相对竖直向上的角度（度）：0 = 竖直，正负表示倾斜方向
    state_.angle = (hardware_.adc.pendulumAngle() - static_cast<float>(cfg::kAngleUpright)) /
                   cfg::kAngleCountsPerDegree;

    state_.angleRate = pendulum_.updateAngleRate(state_.angle);  // 度/秒
    state_.voltage = hardware_.adc.batteryMilliVolt();
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
