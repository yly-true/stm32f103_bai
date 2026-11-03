#ifndef APP_CONTROL_HPP
#define APP_CONTROL_HPP

#include "Hardware/BoardHardware.hpp"
#include "PendulumController/PendulumController.hpp"
#include "ButtonHandler/ButtonHandler.hpp"
#include "ControlState.hpp"

namespace app {

// 顶层控制类：main() 里只需要反复调用 controlLoop()。
//   - 每个 20 ms 节拍：采样 → 保护/控制 → 输出 → 按键扫描
// 状态（ControlState）和硬件（BoardHardware）都是它的私有成员，外部只能通过 controlLoop()
// 和中断节拍入口接触它。
class Control {
public:
    static Control& instance();

    // 由 main 调用，内部获取单例；没有新节拍时立即返回。
    static void controlLoop();

    // TIM1 中断只负责置一个标志
    void onTickInterrupt();

    // USART1 DMA 格式化打印：最多 127 字节，不含结尾 '\0'。
    // 返回已提交字节数；忙、格式化、超长或启动失败返回 -1。
    // 仅由主循环调用；未发完时不等待、不覆盖，也不排队。
    // 当前 nano printf 未启用 %f 浮点格式。
    int uart1Printf(const char* format, ...);

private:
    void sample();

    ControlState       state_;
    BoardHardware      hardware_;
    PendulumController pendulum_;
    ButtonHandler      buttons_;
    char               uart1TxBuffer_[128] = {};  // DMA 完成前持续有效
};

} // namespace app

#endif
