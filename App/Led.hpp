#ifndef APP_LED_HPP
#define APP_LED_HPP

#include "main.h"

namespace app {

// LED：PA4，低电平点亮。只负责指示，不参与控制。
class Led {
public:
    constexpr Led() = default;

    void on() const     { LED = 0; }
    void off() const    { LED = 1; }
    void toggle() const { LED = !LED; }

    // 每调用 period 次翻转一次；period<=0 表示常亮
    void blink(int period) {
        if (period <= 0) { LED = 0; return; }
        if (++counter_ == period) { LED = !LED; counter_ = 0; }
    }

private:
    int counter_ = 0;
};

} // namespace app

#endif
