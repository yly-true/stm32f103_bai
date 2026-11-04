#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

// 控制参数沿用厂家原版，控制节拍与厂家原版一致（5 ms）。
// 量纲约定：
//   角度     —— 度，相对竖直向上，正负表示倾斜方向（0 = 竖直）
//   角速度   —— 度/秒
//   位置     —— 编码器脉冲（TIM4 4 倍频计数）
//   PWM 输出 —— TIM3 比较值，7200 = 100% 占空比
namespace cfg {

// ---- 机械与传感器基准 ----
constexpr int   kEncoderPowerOn   = 10000;  // 上电时 TIM4->CNT 预置值

// 角度刻度：竖直向上 3100、竖直向下 1020，两点相差 180°
constexpr int   kAngleUpright     = 3100;
constexpr int   kAngleDown        = 1020;
constexpr float kAngleCountsPerDegree = (kAngleUpright - kAngleDown) / 180.0f;  // ≈ 11.56 码/度

// ---- 竖直窗口（滞回，单位：度）----
// 只有 |angle| <= kBalanceEnterAngleDeg 才允许进入 PID；
// 进入后要超出更宽的 kBalanceExitAngleDeg 才退出（约等于原来的 ±300 / ±600 码）。
constexpr float kBalanceEnterAngleDeg = 26.0f;
constexpr float kBalanceExitAngleDeg  = 52.0f;

// ---- 输出限制 ----
constexpr int   kPwmPeak          = 7200;   // TIM3 ARR+1，10 kHz
constexpr int   kPwmLimit         = 6900;   // 平衡输出上限（正负对称）

// ---- 电源保护 ----
constexpr int   kBatteryMinMv     = 700;    // 7.00 V 以下停机

// ---- 时序 ----
constexpr int   kControlPeriodMs  = 5;       // TIM1 中断周期，需与 Platform/tim.c 和 WHEELTEC.ioc 同步
constexpr float kControlPeriodS   = kControlPeriodMs / 1000.0f;          // 0.005 s
constexpr int   kPositionDivider  = 6;       // 位置环每 6 拍更新一次 = 30 ms
constexpr float kPositionPeriodS  = kPositionDivider * kControlPeriodS;  // 0.03 s
constexpr int   kRateSampleDiv    = 6;       // 角速度差分周期：6 × 5 ms = 30 ms
constexpr float kRatePeriodS      = kRateSampleDiv * kControlPeriodS;    // 0.03 s
constexpr int   kRateFilterLen    = 4;       // 角速度滑动平均长度
constexpr int   kTelemetryDivider = 10;      // 串口上报周期：10 × 5 ms = 50 ms

// ---- PID 增益（物理单位，换算自厂家 5 ms 原始系数）----
// 厂家原值：角度 400 / 400（码、码/5ms），位置 20 / 300（脉冲、脉冲/30ms）。
// 微分项已按秒归一化，改控制周期不用重调 Kd。
constexpr float kAngleKp = 400.0f * kAngleCountsPerDegree;                  // PWM / 度
constexpr float kAngleKd = 400.0f * kControlPeriodS * kAngleCountsPerDegree; // PWM / (度/秒)

constexpr float kPositionKp = 20.0f;                        // PWM / 编码器脉冲
constexpr float kPositionKd = 300.0f * kPositionPeriodS;    // PWM / (脉冲/秒) = 9

// 位置环一阶低通：τ = 0.12 s，在 30 ms 步长下等价于原来的 0.8/0.2
constexpr float kPositionFilterTauS  = 0.12f;
constexpr float kPositionFilterAlpha = kPositionPeriodS / (kPositionFilterTauS + kPositionPeriodS);

// 启动键消抖：按下和松开都连续采样 2 次确认；急停不等待消抖。
constexpr int   kKeyDebounceTicks = 2;

} // namespace cfg

#endif
