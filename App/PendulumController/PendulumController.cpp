#include "PendulumController.hpp"

namespace app {

float PendulumController::updateAngleRate(float angle)
{
    if (++rateDivider_ >= cfg::kRateSampleDiv) {
        const int delta = static_cast<int>(angle - lastAngle_);
        lastAngle_ = angle;
        rateDivider_ = 0;
        for (int i = 1; i < cfg::kRateFilterLen; ++i) {
            rateBuffer_[i - 1] = rateBuffer_[i];
        }
        rateBuffer_[cfg::kRateFilterLen - 1] = static_cast<short>(delta);
        int sum = 0;
        for (int value : rateBuffer_) sum += value;
        angleRate_ = static_cast<float>(static_cast<short>(sum / cfg::kRateFilterLen));
    }
    return angleRate_;
}

bool PendulumController::nearUpright(const ControlState& s, int window)
{
    const float upright = static_cast<float>(cfg::kAngleUpright);
    const float w = static_cast<float>(window);
    return s.angle <= upright + w && s.angle >= upright - w;
}

void PendulumController::clearOutput(ControlState& s)
{
    s.motorPwm = s.balancePwm = s.positionPwm = 0;
    s.divider = 0;
}

void PendulumController::tick(ControlState& s, BoardHardware& io)
{
    // 低压保护：任何模式下都直接回停机，之后只能重新按启动键。
    if (s.voltage < cfg::kBatteryMinMv) {
        s.mode = RunMode::kStopped;
        clearOutput(s);
        anglePd_.reset();
        positionPd_.reset();
        io.motor.brake();
        return;
    }

    // ① 停机：不输出，等启动键
    if (s.mode == RunMode::kStopped) {
        clearOutput(s);
        io.motor.brake();
        return;
    }

    // ② 已启动，等摆杆进入竖直窗口；这段时间不输出 PID，可以手动扶摆
    if (s.mode == RunMode::kArmed) {
        clearOutput(s);
        io.motor.brake();
        if (nearUpright(s, cfg::kAngleEnterWindow)) {
            anglePd_.reset();
            positionPd_.reset();
            s.positionZero = s.encoder;  // 以当前位置为位置环原点，避免一进入就猛拉
            s.mode = RunMode::kBalancing;
        }
        return;
    }

    // ③ 平衡中：先判断是否已经掉出较宽的退出窗口（滞回）
    //    掉出去就回停机，必须重新按启动键，避免摆杆高速穿过窗口时被 PID 猛踢。
    if (!nearUpright(s, cfg::kAngleExitWindow)) {
        clearOutput(s);
        anglePd_.reset();
        positionPd_.reset();
        s.mode = RunMode::kStopped;
        io.motor.brake();
        return;
    }

    balance(s, io);
}

void PendulumController::balance(ControlState& s, BoardHardware& io)
{
    s.balancePwm = anglePd_.update(s.angle, cfg::kBalanceKpDefault, cfg::kBalanceKdDefault);

    if (++s.divider > cfg::kPositionDivider) {
        s.positionPwm = positionPd_.update(s.encoder, s.positionZero,
                                           cfg::kPositionKpDefault, cfg::kPositionKdDefault);
        s.divider = 0;
    }

    s.motorPwm = limit(s.balancePwm - s.positionPwm);
    io.motor.setPwm(s.motorPwm);
}

int PendulumController::limit(int pwm, int upper, int lower)
{
    if (pwm > upper) return upper;
    if (pwm < -lower) return -lower;
    return pwm;
}

} // namespace app
