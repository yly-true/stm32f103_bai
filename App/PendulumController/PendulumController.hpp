#ifndef APP_PENDULUM_CONTROLLER_HPP
#define APP_PENDULUM_CONTROLLER_HPP

#include "Hardware/BoardHardware.hpp"
#include "Pid.hpp"
#include "Control/ControlState.hpp"

namespace app {

// 摆杆控制：角速度估计、手动平衡和自动起摆。
// 自动起摆过程：
//   第 0 步 Find_Zero —— 把滑块拉回轨道中间，等摆杆静止在"竖直向下"
//   第 1 步 Auto_run  —— 先来回甩杆积累能量，判定能站住后切平衡控制并爬升 PID
class PendulumController {
public:
    void tick(ControlState& s, BoardHardware& io);
    float updateAngleRate(float angle);

private:
    void updateBalance(ControlState& s, BoardHardware& io);
    void protect(ControlState& s, const BoardHardware& io) const;
    void updateSwingUp(ControlState& s, BoardHardware& io);
    void findZero(ControlState& s);
    void autoRun(ControlState& s, BoardHardware& io);
    static int limit(int pwm, int upper = cfg::kPwmLimit, int lower = cfg::kPwmLimit);

    // 两种控制模式共享同一组 PD 历史。
    AnglePd anglePd_;
    PositionPd positionPd_;

    // 角速度差分与滑动平均历史。
    int rateDivider_ = 0;
    float lastAngle_ = 0.0f;
    float angleRate_ = 0.0f;
    short rateBuffer_[cfg::kRateFilterLen] = {};

    // 第 0 步
    bool  step0Done_ = false;
    bool  step1Active_ = false;
    float settleCount_ = 0.0f;
    int   targetPosition_ = cfg::kPositionMiddle;

    // 第 1 步（甩杆）
    IncrementalPi   incrementalPi_;
    PrePositionPd   prePosition_;
    bool  directionLeft_ = false;
    bool  directionRight_ = false;
    int   speed_ = 0;
    long  successCount_ = 0;

    // 第 1 步（起摆成功后的接管与参数爬升）
    bool  successFlag_ = false;
    long  waitCount_ = 0;
    int   helpCount_ = 0;
    long  pidAdjust_ = 0;
};

} // namespace app

#endif
