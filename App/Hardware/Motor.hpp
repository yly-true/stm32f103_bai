#ifndef APP_MOTOR_HPP
#define APP_MOTOR_HPP

#include "main.h"

namespace app {

// 电机：一路 PWM + 两个方向脚 + 自带编码器。
//   PWM   : TIM3_CH4 -> PB1（10 kHz）
//   方向  : BIN1=PB13，BIN2=PB12（一高一低决定正反转）
//   编码器: TIM4 的 A/B 相（PB6/PB7），4 倍频计数
// 说明：这台倒立摆用的是带霍尔编码器的直流减速电机；若换成无刷+驱动器，
//       只需替换本类的 setPwm/position 实现，上层逻辑不用动。
class Motor {
public:
    static constexpr int kPwmPeak = 7200;   // TIM3 ARR+1

    // pwm 取正负表示方向，绝对值是占空比
    void setPwm(int pwm) const {
        if (pwm < 0) { BIN2 = 1; BIN1 = 0; }
        else         { BIN2 = 0; BIN1 = 1; }
        PWMB = static_cast<uint32_t>(pwm < 0 ? -pwm : pwm);
    }

    // 两路输入都拉低，电机自由滑行
    void brake() const { BIN1 = 0; BIN2 = 0; }

    // 占空比 0，方向保持"正"
    void stop() const { setPwm(0); }

    // 编码器累计脉冲，上电时 TIM4->CNT 预置为 10000
    int position() const { return Read_Encoder(4); }

    // 把当前计数值当作新的零点（换向/重新对中时用）
    void setPosition(int value) const { TIM4->CNT = static_cast<uint32_t>(value); }
};

} // namespace app

#endif
