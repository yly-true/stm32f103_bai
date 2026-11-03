#ifndef APP_CONTROL_HPP
#define APP_CONTROL_HPP

#include "Balance.hpp"
#include "Board.hpp"
#include "Calibration.hpp"
#include "Keys.hpp"
#include "RateEstimator.hpp"
#include "State.hpp"
#include "SwingUp.hpp"

namespace app {

// 顶层控制类：main() 里只需要反复调用 controlLoop()。
//   - 每个 5 ms 节拍：采样 → 保护/控制 → 输出 → 按键扫描
//   - 每 10 个节拍（50 ms）：OLED 刷新 + 上位机上报
// 状态（State）和硬件（Board）都是它的私有成员，外部只能通过 controlLoop()
// 和只读访问器接触它。
class Control {
public:
    static Control& instance();

    // 由 main 的超级循环调用；没有新节拍时立即返回。
    void controlLoop();

    // TIM1 中断只负责置一个标志
    void onTickInterrupt();

    // 给 C 显示层用的只读视图
    const State& state() const { return state_; }

private:
    void sample();
    void dispatch();
    void scanKeys();                     // 5 ms 轮询启停/菜单/PID 四个键
    void handleKey(uint16_t pressed);    // 处理新按下的键（位掩码）
    void serviceUi();

    State         state_;
    Board         board_;
    Balance       balance_;
    SwingUp       swingUp_;
    Keys          keys_;
    Calibration   calibration_;
    RateEstimator rateEstimator_;
    uint8_t       uiDivider_ = 0;

    // 按键轮询的消抖状态
    uint16_t rawKey_ = 0;                // 本拍原始电平
    uint16_t stableKey_ = 0;             // 消抖后的稳定状态
    uint16_t prevKey_ = 0;               // 上一拍的稳定状态，用于求边沿
    uint8_t  keyStableCount_ = 0;
};

} // namespace app

#endif
