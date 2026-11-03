#ifndef APP_RATE_ESTIMATOR_HPP
#define APP_RATE_ESTIMATOR_HPP

#include "Config.hpp"

namespace app {

// 角速度估计：每 6 个控制周期做一次差分，再用 4 点滑动平均平滑。
// 与原 Get_D_Angle_Balance() / Mean_Filter() 完全一致（包括丢给小数的截断行为）。
class RateEstimator {
public:
    float update(float angle) {
        if (++divider_ > cfg::kRateSampleDiv - 1) {
            const int delta = static_cast<int>(angle - lastAngle_);
            lastAngle_ = angle;
            divider_ = 0;
            return filter(delta);
        }
        return rate_;
    }

    void reset() {
        divider_ = 0;
        lastAngle_ = 0.0f;
        rate_ = 0.0f;
        for (int i = 0; i < cfg::kRateFilterLen; ++i) buf_[i] = 0;
    }

private:
    int filter(int value) {
        for (int i = 1; i < cfg::kRateFilterLen; ++i) {
            buf_[i - 1] = buf_[i];
        }
        buf_[cfg::kRateFilterLen - 1] = static_cast<short>(value);
        int sum = 0;
        for (int i = 0; i < cfg::kRateFilterLen; ++i) sum += buf_[i];
        rate_ = static_cast<float>(static_cast<short>(sum / cfg::kRateFilterLen));
        return static_cast<int>(rate_);
    }

    int   divider_ = 0;
    float lastAngle_ = 0.0f;
    float rate_ = 0.0f;
    short buf_[cfg::kRateFilterLen] = {0, 0, 0, 0};
};

} // namespace app

#endif
