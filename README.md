# STM32F103 直线倒立摆

WHEELTEC IP570 固件，基于 STM32F103C8T6、HAL 和 C++17。TIM1 提供 **5 ms（200 Hz）** 控制节拍，无显示屏。

## 使用

- **PA5：启动/解锁**。上电默认停机，先松开再按下启动键。
- **PA2：软件急停**。清零 PWM 并回到停机；松开后需重新按启动键，急停优先。
- 已移除自动起摆、菜单、PID 按键调参、单双击、长按和 OLED 功能。

## 运行模式

状态机只有三态（`App/Control/ControlState.hpp` 的 `RunMode`）：

```text
kStopped ──按启动键──▶ kArmed ──摆进竖直窗口 ±26°──▶ kBalancing
    ▲                                                   │
    └───────急停 / 低压 / 掉出窗口 ±52°（需重新按键）────┘
```

- **kStopped**：上电/急停/低压，电机不输出（自由滑行）。
- **kArmed**：已按启动键，等待摆杆进入竖直窗口；电机不输出，可以手动把摆杆扶到竖直。
- **kBalancing**：`|angle| ≤ 26°` 才进入，角度环 + 位置环输出；`|angle| > 52°` 退出并回到 kStopped，需要重新按启动键。
  两个窗口宽度在 `App/Control/Config.hpp` 的 `kBalanceEnterAngleDeg` / `kBalanceExitAngleDeg` 中修改（单位：度）。

进入平衡时位置环目标取当前滑块位置，不会一接管就把滑块拽回中点。低压（< 7.00 V）在任何模式下都直接回停机。

软件急停受主循环耗时影响，不等同于硬件断电急停。当前参数尚需上板验证。

## 控制参数

单位约定（见 `App/Control/Config.hpp`）：

| 量 | 单位 |
|---|---|
| 角度 | 度，相对竖直向上（0 = 竖直，正负表示倾斜方向） |
| 角速度 | 度/秒 |
| 位置 | 编码器脉冲（TIM4 4 倍频计数） |
| PWM | TIM3 比较值，7200 = 100% 占空比 |

角度刻度由 `kAngleUpright = 3100`、`kAngleDown = 1020`（相差 180°）标定，得到 **11.56 码/度**。

角度环（5 ms）和位置环（30 ms）共用 `App/PendulumController/Pid.hpp` 里同一个 `Pid` 类，只是增益不同。微分项显式除以 `dt`，所以改控制周期不需要重调 `Kd`。

| 增益 | 量纲 | 数值（由厂家 5 ms 原值换算） |
|---|---|---|
| `kAngleKp` | PWM / 度 | ≈ 4622 |
| `kAngleKd` | PWM / (度/秒) | ≈ 23.1 |
| `kPositionKp` | PWM / 脉冲 | 20 |
| `kPositionKd` | PWM / (脉冲/秒) | 9 |

位置通道目前用编码器脉冲：厂家资料没有给出“脉冲 → 毫米”的标定。若实测出 `counts/mm`，把两个位置增益按同一系数换算即可（`Kp[PWM/mm] = Kp[PWM/脉冲] × counts/mm`）。

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

主循环每 50 ms 上报一次运行模式：`0` 停机、`1` 等竖直、`2` 平衡中。
