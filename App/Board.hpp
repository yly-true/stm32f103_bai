#ifndef APP_BOARD_HPP
#define APP_BOARD_HPP

#include "Adc.hpp"
#include "Led.hpp"
#include "Motor.hpp"

namespace app {

// 板级对象：应用层只认这三样硬件，不直接碰寄存器和宏。
struct Board {
    Led   led;
    Motor motor;
    Adc   adc;
};

} // namespace app

#endif
