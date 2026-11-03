#ifndef APP_KEYS_HPP
#define APP_KEYS_HPP

#include "Board.hpp"
#include "State.hpp"

namespace app {

// 按键扫描。两种节奏：
//   tick5ms()  —— 在控制中断里跑，处理用户键的单击/双击（调目标位置）和长按（切模式）
//   click()    —— 给校准向导用，轮询启停/加/减三个键
// 按键是纯输入信号，不属于 LED/电机/ADC 三类硬件，所以直接读引脚宏。
class Keys {
public:
    void tick5ms(State& s);
    void updateIndicator(const State& s, Board& io);

    int  click();          // 返回 5 / 11 / 12，无按键返回 0
    bool longPressMenu();  // 菜单键长按（调用者按 50 ms 节奏调用，>20 次约 1 s）

private:
    int  clickSingleOrDouble(int window);

    // clickSingleOrDouble 内部状态
    bool    armed_ = false;
    bool    pressed_ = false;
    int     pending_ = 0;
    int     single_ = 0;
    int     forever_ = 0;

    // tick5ms 用到的位置微调状态
    int     stepFlag_ = 0;
    int     stepCount_ = 0;

    // 长按计数
    int     holdUser_ = 0;
    bool    holdUserArmed_ = true;
    int     holdMenu_ = 0;
    bool    holdMenuFired_ = false;
    bool    clickHeld_ = true;
};

} // namespace app

#endif
