/**
 * @file    Control.cpp
 * @brief   顶层控制类：按键 → 采样 → 控制 → 状态输出
 *
 * 时间线：
 *   TIM1 每 5 ms 置 tickPending  →  main 循环调用 controlLoop()
 * 启动键每 5 ms 消抖，软件急停每轮主循环检查；USART1 DMA 发送日志（50 ms 一次）。
 *
 * 运行模式（ControlState.hpp 的 RunMode）：
 *   kStopped   上电/急停/低压，电机不输出
 *   kArmed     已按启动键，等摆杆进入竖直窗口，电机不输出
 *   kBalancing 竖直窗口内，PID 输出
 * 串口每 50 ms 上报编码器、角度环 PWM、位置环 PWM 和角度。
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
    control.handleEmergencyStop();

    if (!control.state_.tickPending) return;
    control.state_.tickPending = 0;

    control.buttons_.scan(control.state_);
    control.sample();

    // ADC 采样期间也可能按下急停，输出前再确认一次。
    if (control.handleEmergencyStop()) return;
    control.pendulum_.tick(control.state_, control.hardware_);
    control.updateIndicators();
    control.sendTelemetry();
}

bool Control::handleEmergencyStop()
{
    if (!buttons_.checkEmergencyStop(state_)) return false;
    hardware_.motor.stop();
    hardware_.led.off();
    return true;
}

void Control::updateIndicators()
{
    switch (state_.mode) {
    case RunMode::kStopped:
        hardware_.led.off();
        break;
    case RunMode::kArmed:          // 慢闪（2 s）：等摆杆竖直
        hardware_.led.blink(400);
        break;
    case RunMode::kBalancing:      // 快闪（0.5 s）：PID 平衡中
        hardware_.led.blink(100);
        break;
    }
}

void Control::sendTelemetry()
{
    // 串口按 50 ms 上报，避免 5 ms 节拍把 DMA 占满
    if (++telemetryDivider_ >= cfg::kTelemetryDivider) {
        telemetryDivider_ = 0;
        // 调试用：encoder, balancePwm, positionPwm, angle（逗号分隔，FireWater 画四条线）
        uart1Printf("%d,%d,%d,%.1f,%d\r\n", state_.encoder,
                    state_.balancePwm, state_.positionPwm, state_.angle, state_.positionZero);
    }
}

void Control::sample()
{
    // 以上电位置为零点，保留原始编码器计数的增减方向。
    state_.encoder = hardware_.motor.position() - cfg::kEncoderPowerOn;

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
