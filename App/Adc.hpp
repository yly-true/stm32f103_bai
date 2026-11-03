#ifndef APP_ADC_HPP
#define APP_ADC_HPP

#include "main.h"

namespace app {

// ADC：两个通道，各自带固定用途。
//   ADC1_IN3 (PA3)：摆杆角度电位器，0~4095 对应角度
//   ADC2_IN6 (PA6)：电池分压，换算成 mV
class Adc {
public:
    static constexpr int kAngleChannel   = 3;
    static constexpr int kBatteryChannel = 6;

    // 摆杆角度：默认取 10 次平均（控制周期内用）
    float pendulumAngle(int samples = 10) const {
        return static_cast<float>(Get_Adc_Average(kAngleChannel, samples));
    }

    // 电池电压，单位 mV（内部已按 3.3V × 11 分压换算）
    int batteryMilliVolt() const {
        return Get_battery_volt();
    }

    // 校准向导需要读更多次，单独开放原始采样
    float raw(int channel, int samples) const {
        return static_cast<float>(Get_Adc_Average(channel, samples));
    }
};

} // namespace app

#endif
