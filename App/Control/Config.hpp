#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

// 控制参数沿用厂家原版；控制节拍与厂家原版一致，为 5 ms。
// 单位说明：角度都是 ADC 原始值（0~4095），位置都是编码器脉冲数。
namespace cfg {

// ---- 机械与传感器基准 ----
constexpr int   kEncoderPowerOn   = 10000;  // 上电时 TIM4->CNT 预置值
constexpr int   kAngleUpright     = 3100;   // 摆杆竖直向上时的 ADC 值（平衡点）

// ---- 竖直窗口（滞回）----
// 只有在 |angle - kAngleUpright| <= kAngleEnterWindow 时才允许进入 PID；
// 进入后要等到超出更宽的 kAngleExitWindow 才退出，避免边界处反复切换。
constexpr int   kAngleEnterWindow = 300;
constexpr int   kAngleExitWindow  = 600;

// ---- 输出限制 ----
constexpr int   kPwmPeak          = 7200;   // TIM3 ARR+1，10 kHz
constexpr int   kPwmLimit         = 6900;   // 平衡输出上限（正负对称）

// ---- 电源保护 ----
constexpr int   kBatteryMinMv     = 700;    // 7.00 V 以下停机

// ---- 时序 ----
constexpr int   kControlPeriodMs  = 5;       // TIM1 中断周期，需与 Platform/tim.c 和 WHEELTEC.ioc 同步
constexpr int   kPositionDivider  = 5;       // 位置环 ++divider > 5：每 6 拍执行一次 = 30 ms
constexpr int   kRateSampleDiv    = 6;       // 角速度差分周期：6 × 5 ms = 30 ms
constexpr int   kRateFilterLen    = 4;       // 角速度滑动平均长度
constexpr int   kTelemetryDivider = 10;      // 串口上报周期：10 × 5 ms = 50 ms

// ---- PID 参数 ----
constexpr float kBalanceKpDefault = 400.0f;
constexpr float kBalanceKdDefault = 400.0f;
constexpr float kPositionKpDefault = 20.0f;
constexpr float kPositionKdDefault = 300.0f;

// 位置环一阶低通系数
constexpr float kPositionFilterOld = 0.8f;
constexpr float kPositionFilterNew = 0.2f;

// 启动键消抖：按下和松开都连续采样 2 次确认；急停不等待消抖。
constexpr int   kKeyDebounceTicks = 2;

} // namespace cfg

#endif
