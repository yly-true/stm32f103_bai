#include "SwingUp.hpp"

#include "Config.hpp"

namespace app {

void SwingUp::reset()
{
    step0Done_ = false;
    step1Active_ = false;
    settleCount_ = 0.0f;
    targetPosition_ = cfg::kPositionMiddle;
    incrementalPi_.reset();
    prePosition_.reset();
    directionLeft_ = false;
    directionRight_ = false;
    speed_ = 0;
    successCount_ = 0;
    successFlag_ = false;
    waitCount_ = 0;
    helpCount_ = 0;
    pidAdjust_ = 0;
}

int SwingUp::limit(int pwm, int upper, int lower)
{
    if (pwm > upper) return upper;
    if (pwm < -lower) return -lower;
    return pwm;
}

void SwingUp::tick(State& s, Board& io, AnglePd& anglePd, PositionPd& positionPd)
{
    findZero(s, io, anglePd);
    if (step0Done_) {
        autoRun(s, io, anglePd, positionPd);
    }

    s.motorPwm = limit(s.motorPwm, cfg::kPwmLimit, cfg::kPwmLimit);

    // 甩杆阶段轨道边界保护
    if (!step1Active_ && (s.encoder >= cfg::kTrackEncoderHi || s.encoder <= cfg::kTrackEncoderLo)) {
        io.motor.stop();
        s.stopped = true;
        return;
    }

    // 起摆成功后的角度/轨道保护
    const bool angleBad = !(s.angle > static_cast<float>(cfg::kAngleUpright - 600) &&
                            s.angle < static_cast<float>(cfg::kAngleUpright + 600));
    if (successFlag_ && (angleBad || s.encoder >= cfg::kTrackEncoderHi || s.encoder <= cfg::kTrackEncoderLo)) {
        io.motor.stop();
        s.stopped = true;
        return;
    }

    io.motor.setPwm(s.motorPwm);
}

// ---------------------------------------------------------------- 第 0 步 ----
void SwingUp::findZero(State& s, Board& io, AnglePd& anglePd)
{
    if (!step0Done_) {
        s.positionZero = cfg::kPositionMiddle;

        // 顺摆姿态下先把摆杆稳住：把"竖直向下"的 1020 折算到平衡点 3100
        s.balancePwm = anglePd.update(s.angle + static_cast<float>(cfg::kAngleUpsideDown),
                                      s.balanceKp, s.balanceKd) / 8;
        s.positionPwm = prePosition_.update(s.encoder, s.positionZero);

        if (s.encoder < cfg::kPositionMiddle + 25) {
            s.motorPwm = incrementalPi_.update(s.encoder, cfg::kPositionMiddle);
        } else {
            s.motorPwm = -s.balancePwm + s.positionPwm;
        }

        const float down = static_cast<float>(cfg::kAngleDown);
        const bool nearDown = s.angle < down + 300.0f && s.angle > down - 300.0f;
        const bool atMiddle = s.encoder > cfg::kPositionMiddle - 50 && s.encoder < cfg::kPositionMiddle + 50;
        if (nearDown && atMiddle) settleCount_ += 1.0f;
        if (nearDown) settleCount_ += 0.1f;

        if (settleCount_ > static_cast<float>(cfg::kSwingUpSettleCount)) {
            step0Done_ = true;
            step1Active_ = false;
            s.motorPwm = 0;
            targetPosition_ = cfg::kPositionMiddle - 668;
        }
    }

    // 归中阶段限速，避免位置闭环把滑块拽飞
    if (s.motorPwm > cfg::kSwingUpSpeedMax)  s.motorPwm = cfg::kSwingUpSpeedMax;
    if (s.motorPwm < -cfg::kSwingUpSpeedMax) s.motorPwm = -cfg::kSwingUpSpeedMax;
    io.motor.setPwm(s.motorPwm);
}

// ---------------------------------------------------------------- 第 1 步 ----
void SwingUp::autoRun(State& s, Board& io, AnglePd& anglePd, PositionPd& positionPd)
{
    const float down = static_cast<float>(cfg::kAngleDown);
    const float upright = static_cast<float>(cfg::kAngleUpright);

    if (!step1Active_) {
        // 1) 按摆杆当前运动方向决定往哪边甩
        if (s.angle > down - 120.0f && s.angle < down + 120.0f) {
            if (s.angleRate <= 0.0f) directionRight_ = true;
            else                     directionLeft_ = true;
        }

        // 2) 摆杆回到原点附近时给出甩杆目标位置
        if (directionLeft_) {
            if (s.angle > down - 50.0f && s.angle < down + 50.0f) {
                directionLeft_ = false;
                targetPosition_ = cfg::kPositionMiddle + 800;
                if (speed_ > 1) targetPosition_ = cfg::kPositionMiddle + 160;
            }
        } else if (directionRight_) {
            if (s.angle > down - 50.0f && s.angle < down + 50.0f) {
                directionRight_ = false;
                targetPosition_ = cfg::kPositionMiddle - 482;
                if (speed_ > 1) targetPosition_ = cfg::kPositionMiddle - 160;
            }
        }

        // 3) 位置闭环：把滑块推到目标点，靠惯性带动摆杆
        s.motorPwm = positionPd.update(s.encoder, targetPosition_, s.positionKp, s.positionKd);

        // 4) 摆杆摆到接近竖直时开始计"接近成功"
        if (s.angle < upright + 385.0f && s.angle > upright - 385.0f) {
            ++speed_;
        }

        const bool canStand = s.angle < upright + static_cast<float>(cfg::kSwingUpAngleWin) &&
                              s.angle > upright - static_cast<float>(cfg::kSwingUpAngleWin) &&
                              s.encoder > cfg::kSwingUpEncoderLo && s.encoder < cfg::kSwingUpEncoderHi &&
                              s.angleRate > -static_cast<float>(cfg::kSwingUpRateWin) &&
                              s.angleRate < static_cast<float>(cfg::kSwingUpRateWin);
        if (canStand) {
            ++speed_;
            ++successCount_;
        } else {
            successCount_ = 0;
        }

        // 5) 连续满足 3 次：认为可以接管
        if (successCount_ > cfg::kSwingUpSuccessHits) {
            step1Active_ = true;
            successFlag_ = true;
            s.balanceKp = cfg::kSwingUpBalanceKp;
            s.balanceKd = cfg::kSwingUpBalanceKd;
            s.positionKp = cfg::kSwingUpPositionKp;
            s.positionKd = cfg::kSwingUpPositionKd;
        }

        s.motorPwm = limit(s.motorPwm, cfg::kSwingUpPosMax, cfg::kSwingUpNegMax);
        return;
    }

    if (successFlag_) {
        // 6) 接管：切到平衡控制，目标位置取当前滑块位置，再缓慢挪回轨道中间
        if (waitCount_ == 0) {
            s.positionZero = s.encoder;
        }

        s.balancePwm = anglePd.update(s.angle, s.balanceKp, s.balanceKd);
        if (++s.divider > cfg::kPositionDivider) {
            s.positionPwm = positionPd.update(s.encoder, s.positionZero, s.positionKp, s.positionKd);
            s.divider = 0;
        }

        ++waitCount_;
        if (waitCount_ > 100 && waitCount_ < 2000) {
            if (s.positionZero > 8100)      --s.positionZero;
            else if (s.positionZero < 8100) ++s.positionZero;
        }
        if (waitCount_ > 2000) waitCount_ = 2001;

        // 7) 每 0.5 s 把 PID 往稳摆参数爬一档，直到恢复默认值
        if (helpCount_ == 0) {
            ++pidAdjust_;
            if (pidAdjust_ % 100 == 0) {
                s.balanceKp += 10.0f;
                s.balanceKd += 10.0f;
                s.positionKp += 0.5f;
                s.positionKd += 10.0f;
            }
            if (s.balanceKp > cfg::kBalanceKpDefault)   s.balanceKp = cfg::kBalanceKpDefault;
            if (s.balanceKd > cfg::kBalanceKdDefault)   s.balanceKd = cfg::kBalanceKdDefault;
            if (s.positionKp > cfg::kPositionKpDefault) s.positionKp = cfg::kPositionKpDefault;
            if (s.positionKd > cfg::kPositionKdDefault) s.positionKd = cfg::kPositionKdDefault;
            if (s.balanceKp == cfg::kBalanceKpDefault && s.balanceKd == cfg::kBalanceKdDefault &&
                s.positionKp == cfg::kPositionKpDefault && s.positionKd == cfg::kPositionKdDefault) {
                helpCount_ = 1;
            }
        }

        s.motorPwm = s.balancePwm - s.positionPwm;
    }
}

} // namespace app
