#include "PendulumController.hpp"

namespace app {

float PendulumController::updateAngleRate(float angle)
{
    if (++rateDivider_ >= cfg::kRateSampleDiv) {
        const float delta = angle - lastAngle_;   // 30 ms 内的角度变化（度）
        lastAngle_ = angle;
        rateDivider_ = 0;
        for (int i = 1; i < cfg::kRateFilterLen; ++i) {
            rateBuffer_[i - 1] = rateBuffer_[i];
        }
        rateBuffer_[cfg::kRateFilterLen - 1] = delta;

        float sum = 0.0f;
        for (float value : rateBuffer_) sum += value;
        angleRate_ = (sum / cfg::kRateFilterLen) / cfg::kRatePeriodS;  // 度/秒
    }
    return angleRate_;
}

bool PendulumController::nearUpright(const ControlState& s, float windowDeg)
{
    return s.angle <= windowDeg && s.angle >= -windowDeg;
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
        anglePid_.reset();
        positionPid_.reset();
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
        if (nearUpright(s, cfg::kBalanceEnterAngleDeg)) {
            anglePid_.reset();
            positionPid_.reset();
            // 对齐官方：以进入平衡时的位置为目标，误差从 0 起步。
            // 这样位置环在启动瞬间不起作用，可以直接判断"往一边跑"是否来自位置环。
            s.positionZero = s.encoder;
            positionFilter_ = 0.0f;
            s.mode = RunMode::kBalancing;
        }
        return;
    }

    // ③ 平衡中：先判断是否已经掉出较宽的退出窗口（滞回）
    //    掉出去就回停机，必须重新按启动键，避免摆杆高速穿过窗口时被 PID 猛踢。
    if (!nearUpright(s, cfg::kBalanceExitAngleDeg)) {
        clearOutput(s);
        anglePid_.reset();
        positionPid_.reset();
        s.mode = RunMode::kStopped;
        io.motor.brake();
        return;
    }

    balance(s, io);
}

void PendulumController::balance(ControlState& s, BoardHardware& io)
{
    // 角度环：每个 5 ms 节拍。误差单位：度；输出单位：PWM 计数。
    s.balancePwm = static_cast<int>(
        -anglePid_.update(s.angle, cfg::kAngleKp, 0.0f, cfg::kAngleKd, cfg::kControlPeriodS));

    // 位置环：每 30 ms 节拍。误差先过一阶低通，再交给同一个 Pid。
    if (++s.divider >= cfg::kPositionDivider) {
        s.divider = 0;
        const float raw = static_cast<float>(s.encoder - s.positionZero);
        positionFilter_ += (raw - positionFilter_) * cfg::kPositionFilterAlpha;
        s.positionPwm = static_cast<int>(
            positionPid_.update(positionFilter_, cfg::kPositionKp, 0.0f,
                                cfg::kPositionKd, cfg::kPositionPeriodS));
    }

    // 与官方一致：Moto = Balance_Pwm - Position_Pwm。
    // 位置误差使用相对上电位置的坐标；位置环增益在 Config.hpp 中配置。
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
