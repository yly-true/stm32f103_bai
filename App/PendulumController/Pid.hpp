#ifndef APP_PID_HPP
#define APP_PID_HPP

namespace app {

// 通用 PID：u = kp·e + ki·∫e dt + kd·de/dt，dt 单位：秒。
// 角度环和位置环共用这一个类，只是增益和调用周期不同。
// 微分项显式除以 dt，所以 Kd 的量纲是 "输出 / (误差/秒)"，改控制周期不用重调。
// 第一次 update（reset 之后）不产生微分项：此时 lastError_ 是 0，
// 直接算 (error-0)/dt 会在进入控制的一瞬间打出一个巨大的尖峰。
class Pid {
public:
    void reset() {
        lastError_ = 0.0f;
        integral_ = 0.0f;
        first_ = true;
    }

    float update(float error, float kp, float ki, float kd, float dt) {
        integral_ += error * dt;
        if (integral_ >  kIntegralLimit) integral_ =  kIntegralLimit;
        if (integral_ < -kIntegralLimit) integral_ = -kIntegralLimit;

        if (first_) {
            first_ = false;
            lastError_ = error;
            return kp * error + ki * integral_;   // 首个采样：不算微分
        }

        const float derivative = (error - lastError_) / dt;
        lastError_ = error;
        return kp * error + ki * integral_ + kd * derivative;
    }

private:
    // 积分限幅（单位：误差·秒）。两个环目前 ki = 0，积分项不参与输出。
    static constexpr float kIntegralLimit = 1000.0f;

    float lastError_ = 0.0f;
    float integral_  = 0.0f;
    bool  first_     = true;
};

} // namespace app

#endif
