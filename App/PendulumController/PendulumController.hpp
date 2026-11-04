#ifndef APP_PENDULUM_CONTROLLER_HPP
#define APP_PENDULUM_CONTROLLER_HPP

#include "Hardware/BoardHardware.hpp"
#include "Pid.hpp"
#include "Control/ControlState.hpp"

namespace app {

// 摆杆控制：角速度估计 + 竖直窗口内的平衡 PID。
// 状态机只有三态（见 ControlState.hpp 的 RunMode）：
//   kStopped   —— 不输出，等启动键
//   kArmed     —— 已启动，等摆杆进入竖直窗口；电机不输出，可以手动扶摆
//   kBalancing —— 竖直窗口内，角度环（5 ms）+ 位置环（30 ms）PID 输出
// 进入窗口用 ±kBalanceEnterAngleDeg，退出用更宽的 ±kBalanceExitAngleDeg，形成滞回；
// 掉出窗口即回 kStopped，需要重新按启动键。
class PendulumController {
public:
    void tick(ControlState& s, BoardHardware& io);

    // 输入角度（度），返回角速度（度/秒）。
    float updateAngleRate(float angle);

private:
    static bool nearUpright(const ControlState& s, float windowDeg);
    static void clearOutput(ControlState& s);
    void balance(ControlState& s, BoardHardware& io);
    static int limit(int pwm, int upper = cfg::kPwmLimit, int lower = cfg::kPwmLimit);

    // 两个环共用同一个 Pid 类，只是增益和调用周期不同。
    Pid   anglePid_;
    Pid   positionPid_;
    float positionFilter_ = 0.0f;   // 位置误差一阶低通状态（编码器脉冲）

    // 角速度差分与滑动平均历史。
    int   rateDivider_ = 0;
    float lastAngle_ = 0.0f;
    float angleRate_ = 0.0f;
    float rateBuffer_[cfg::kRateFilterLen] = {};
};

} // namespace app

#endif
