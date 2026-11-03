#ifndef APP_PID_HPP
#define APP_PID_HPP

#include "Config.hpp"

// 四个调节器，全部是"纯函数 + 一点内部状态"，数值与厂家原版逐行对应。
namespace app {

// 角度环：PD，输出取反号（角度偏大 → 往负方向修正）
class AnglePd {
public:
    int update(float angle, float kp, float kd) {
        const float bias = angle - static_cast<float>(cfg::kAngleUpright);
        const float dBias = bias - lastBias_;
        lastBias_ = bias;
        return static_cast<int>(-kp * bias - kd * dBias);
    }
    void reset() { lastBias_ = 0.0f; }

private:
    float lastBias_ = 0.0f;
};

// 位置环：先一阶低通，再 PD。每 25 ms 调用一次。
class PositionPd {
public:
    int update(int encoder, int target, float kp, float kd) {
        const float least = static_cast<float>(encoder - target);
        bias_ = bias_ * cfg::kPositionFilterOld + least * cfg::kPositionFilterNew;
        const float diff = bias_ - lastBias_;
        lastBias_ = bias_;
        return static_cast<int>(bias_ * kp + diff * kd);
    }
    void reset() { bias_ = 0.0f; lastBias_ = 0.0f; }

private:
    float bias_ = 0.0f;
    float lastBias_ = 0.0f;
};

// 增量式 PI：只在起摆归中阶段做位置闭环
class IncrementalPi {
public:
    int update(int encoder, int target) {
        const float bias = static_cast<float>(encoder - target);
        pwm_ += 10.0f * (bias - lastBias_) / 20.0f + 10.0f * bias / 20.0f;
        lastBias_ = bias;
        return static_cast<int>(pwm_);
    }
    void reset() { pwm_ = 0.0f; lastBias_ = 0.0f; }

private:
    float pwm_ = 0.0f;
    float lastBias_ = 0.0f;
};

// 顺摆位置 PD：起摆归中阶段用，系数固定 25 / 600
class PrePositionPd {
public:
    int update(int encoder, int target) {
        const float bias = static_cast<float>(encoder - target);
        const float diff = bias - lastBias_;
        lastBias_ = bias;
        return static_cast<int>(bias * 25.0f + diff * 600.0f);
    }
    void reset() { lastBias_ = 0.0f; }

private:
    float lastBias_ = 0.0f;
};

} // namespace app

#endif
