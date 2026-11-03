#include "Balance.hpp"

#include "Config.hpp"

namespace app {

void Balance::reset()
{
    anglePd_.reset();
    positionPd_.reset();
}

int Balance::limit(int pwm)
{
    if (pwm < -cfg::kPwmLimit) return -cfg::kPwmLimit;
    if (pwm >  cfg::kPwmLimit) return  cfg::kPwmLimit;
    return pwm;
}

// 等价于原 Turn_Off()：进入时若已停机就先刹车，再判断角度/电压是否越界。
void Balance::protect(State& s, const Board& io) const
{
    if (s.stopped) {
        io.motor.brake();
    }
    const bool angleBad = !(s.angle > static_cast<float>(cfg::kAngleUpright - cfg::kAngleHalfRange) &&
                            s.angle < static_cast<float>(cfg::kAngleUpright + cfg::kAngleHalfRange));
    if (angleBad || s.voltage < cfg::kBatteryMinMv) {
        s.stopped = true;
    }
}

void Balance::tick(State& s, Board& io)
{
    protect(s, io);

    // 从起摆模式切回来时，清掉积分并把当前位置当作新目标（原 Swing_up 逻辑）
    if (!s.swingUpFresh) {
        s.positionZero = s.encoder;
        s.divider = 0;
        anglePd_.reset();
        positionPd_.reset();
        s.swingUpFresh = true;
    }

    if (!s.stopped) {
        s.balancePwm = anglePd_.update(s.angle, s.balanceKp, s.balanceKd);

        if (++s.divider > cfg::kPositionDivider) {
            s.positionPwm = positionPd_.update(s.encoder, s.positionZero, s.positionKp, s.positionKd);
            s.divider = 0;
        }

        s.motorPwm = limit(s.balancePwm - s.positionPwm);
        io.motor.setPwm(s.motorPwm);
    }
}

} // namespace app
