# STM32F103 直线倒立摆

WHEELTEC IP570 固件，基于 STM32F103C8T6、HAL 和 C++17。TIM1 提供 **20 ms（50 Hz）** 控制节拍，无显示屏。

## 使用

- **PA5：启动/解锁**。上电默认停机，先松开再按下启动键。
- **PA2：软件急停**。清零 PWM 并回到停机；松开后需重新按启动键，急停优先。
- 已移除自动起摆、菜单、PID 按键调参、单双击、长按和 OLED 功能。

## 运行模式

状态机只有三态（`App/Control/ControlState.hpp` 的 `RunMode`）：

```text
kStopped ──按启动键──▶ kArmed ──摆进竖直窗口 ±300──▶ kBalancing
    ▲                                                   │
    └───────急停 / 低压 / 掉出窗口 ±600（需重新按键）────┘
```

- **kStopped**：上电/急停/低压，电机不输出（自由滑行）。
- **kArmed**：已按启动键，等待摆杆进入竖直窗口；电机不输出，可以手动把摆杆扶到竖直。
- **kBalancing**：`|angle - 3100| ≤ 300` 才进入，角度环 + 位置环输出；`|angle - 3100| > 600` 退出并回到 kStopped，需要重新按启动键。
  两个窗口宽度在 `App/Control/Config.hpp` 的 `kAngleEnterWindow` / `kAngleExitWindow` 中修改。

进入平衡时位置环目标取当前滑块位置，不会一接管就把滑块拽回中点。低压（< 7.00 V）在任何模式下都直接回停机。

软件急停受主循环耗时影响，不等同于硬件断电急停。当前参数尚需上板验证。

## 目录

```text
App/
  main.cpp               外设初始化与主循环
  Control/               顶层调度、运行状态和参数
  PendulumController/    竖直窗口判定、平衡 PID 及角速度估计
  ButtonHandler/         启动与软件急停
  Hardware/              电机、编码器、ADC 和 LED 封装
Platform/                外设初始化、中断和启动文件
Drivers/                 HAL 与 CMSIS
tools/                   工具目录（见下）
  download/              环境下载与配置
  build/                 编译脚本
  flymcu/                串口烧录软件
build/                   自动生成的固件与缓存，不入库
```

## 编译与烧录

Windows 10/11 x64，在工程目录操作。

**首次配置环境（只下载和配置）：**

```powershell
.\tools\download\setup-toolchain.cmd
```

自动配置 CMake、Arm GCC/G++ 和 GNU Make，无需参数或手动设置 PATH。首次需联网，无需管理员权限，也不需要预装 Python、Conda 或 IDE。

**编译：**

```powershell
.\tools\build\wt.cmd
```

默认 Debug，固件生成在 `build/`；已有工具会自动复用。

FlyMcu 烧录 `build/WHEELTEC.hex`：BOOT0=1、BOOT1=0 后复位进入 ISP；烧完将 BOOT0 改回 0，再复位运行。

## 串口

USART1：PA9/PA10，**128000 波特率、8N1、DMA 发送**。主循环中可调用：

```cpp
app::Control::instance().uart1Printf("value=%d\r\n", 123);
```

单次最多 127 字节；忙或失败返回 `-1`，不排队。当前未启用 `%f` 浮点打印。

主循环每 20 ms 上报一次运行模式：`0` 停机、`1` 等竖直、`2` 平衡中。
