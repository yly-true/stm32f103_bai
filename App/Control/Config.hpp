#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

// 控制参数沿用厂家原版；控制节拍已从 5 ms 调整为 20 ms。
// 单位说明：角度都是 ADC 原始值（0~4095），位置都是编码器脉冲数。
namespace cfg {

// ---- 机械与传感器基准 ----
constexpr int   kEncoderPowerOn   = 10000;  // 上电时 TIM4->CNT 预置值
constexpr int   kAngleUpright     = 3100;   // 摆杆竖直向上时的 ADC 值（平衡点）
constexpr int   kAngleDown        = 1020;   // 摆杆竖直向下时的 ADC 值（起摆原点）
constexpr int   kPositionMiddle   = 7925;   // 滑块轨道中点
constexpr int   kAngleHalfRange   = 500;    // 停机角度窗口：|angle-3100| > 500 即保护
constexpr int   kAngleUpsideDown  = 2070;   // 起摆时把"向下"折算到"向上"的偏移

// ---- 输出限制 ----
constexpr int   kPwmPeak          = 7200;   // TIM3 ARR+1，10 kHz
constexpr int   kPwmLimit         = 6900;   // 正常平衡输出上限
constexpr int   kSwingUpSpeedMax  = 2500;   // 归中阶段的速度上限
constexpr int   kSwingUpPosMax    = 4100;   // 甩杆阶段正向上限
constexpr int   kSwingUpNegMax    = 5100;   // 甩杆阶段反向上限

// ---- 电源保护 ----
constexpr int   kBatteryMinMv     = 700;    // 7.00 V 以下停机

// ---- 时序 ----
constexpr int   kControlPeriodMs  = 20;      // TIM1 中断周期，需与 Platform/tim.c 和 WHEELTEC.ioc 同步
constexpr int   kPositionDivider  = 5;      // 位置环使用 ++divider > 5，实际每 6 拍执行一次 = 120 ms
constexpr int   kRateSampleDiv    = 6;      // 角速度差分周期：6 × 20 ms = 120 ms
constexpr int   kRateFilterLen    = 4;      // 角速度滑动平均长度

// ---- PID 初值 ----
constexpr float kBalanceKpDefault = 400.0f;
constexpr float kBalanceKdDefault = 400.0f;
constexpr float kPositionKpDefault = 20.0f;
constexpr float kPositionKdDefault = 300.0f;

// 起摆瞬间使用的一组"软"参数，稳摆后逐级爬回默认值
constexpr float kSwingUpBalanceKp = 210.0f;
constexpr float kSwingUpBalanceKd = 150.0f;
constexpr float kSwingUpPositionKp = 8.0f;
constexpr float kSwingUpPositionKd = 130.0f;

// 位置环一阶低通系数
constexpr float kPositionFilterOld = 0.8f;
constexpr float kPositionFilterNew = 0.2f;

// 启动键消抖：按下和松开都连续采样 2 次确认；急停不等待消抖。
constexpr int   kKeyDebounceTicks = 2;

// ---- 自动起摆过程的判据 ----
constexpr int   kSwingUpSettleCount = 200; // 归中稳定判定次数（约 4 s）
constexpr int   kSwingUpSuccessHits = 3;   // 满足起摆条件次数
constexpr int   kSwingUpAngleWin    = 120; // 平衡点附近窗口
constexpr int   kSwingUpRateWin     = 30;  // 角速度窗口
constexpr int   kSwingUpEncoderLo   = 6300;
constexpr int   kSwingUpEncoderHi   = 9300;
constexpr int   kTrackEncoderLo     = 5900; // 轨道保护边界
constexpr int   kTrackEncoderHi     = 9900;

} // namespace cfg

#endif
