#ifndef APP_CALIBRATION_HPP
#define APP_CALIBRATION_HPP

#include "Board.hpp"
#include "Keys.hpp"
#include "State.hpp"

namespace app {

// 角度传感器校准向导（原 check.c 的 Tips()）。
// 长按菜单键进入，之后屏蔽控制中断、用阻塞式分页向导引导用户把
// "摆杆竖直向下" 时 ADC 调进 1010~1030，期间电机保持停止。
class Calibration {
public:
    // 主循环每 50 ms 调用一次。
    void tick(State& s, Keys& keys, Board& io);

private:
    void enter(State& s, Board& io);
    void leave(State& s);

    enum class Step { Tips, Page0, Page1, Page2, Page3, Check, Success, Fail };

    int  hitCount_ = 0;
    int  sampleCount_ = 0;
    int  countdown_ = 0;
};

} // namespace app

#endif
