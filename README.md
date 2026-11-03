# STM32F103 直线倒立摆

WHEELTEC IP570 直线倒立摆的固件工程，基于 STM32F103C8T6，用 **C++ 重构** 过：

- 硬件抽象成 `Led` / `Motor`（含编码器）/ `Adc` 三个类；
- 控制逻辑收进一个 `Control` 类，`main()` 只负责初始化外设 + 超级循环；
- 控制节拍使用 **TIM1 中断**（每 20 ms 置标志），按键轮询；SysTick 负责 HAL 毫秒计时。

## 硬件

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6，Cortex-M3，72 MHz，64 KB Flash / 20 KB RAM |
| 摆杆角度 | 电位器 → ADC1_IN3（PA3），0~4095，竖直向上约 3100，竖直向下约 1020 |
| 滑块位置 | 增量编码器 → TIM4（PB6/PB7），上电计数预置 10000 |
| 电机 | 直流减速电机 + H 桥：PWM = TIM3_CH4（PB1），方向 = BIN1/BIN2（PB13/PB12） |
| 电池电压 | 分压 → ADC2_IN6（PA6），7.00 V 以下保护停机 |
| 显示 | 无显示屏，已移除 OLED 驱动、页面及校准向导 |
| 串口 | USART1（PA9/PA10），128000 波特，8N1；DMA1 Channel4 发送日志，无 DataScope 上报 |
| 按键 | PA5 启动、PA2 软件急停，低电平有效；其余按键未使用 |

## 目录

```
App/
  main.cpp                         外设初始化与主循环
  Control/
    Control.cpp / Control.hpp      顶层调度：采样、控制、输出
    ControlState.hpp               运行数据和模式
    Config.hpp                     控制参数、阈值与时序
  PendulumController/
    PendulumController.cpp / .hpp  平衡、自动起摆、角速度估计
    Pid.hpp                        PD / PI 调节器
  ButtonHandler/
    ButtonHandler.cpp / .hpp       启动消抖与软件急停
  Hardware/
    BoardHardware.hpp              板级硬件汇总
    Adc.hpp / Motor.hpp / Led.hpp   ADC、电机和 LED 封装

Platform/               CubeMX 生成的外设初始化、中断、启动文件、系统文件
Drivers/                STM32F1 HAL + CMSIS
docs/                   工具链说明、历史交接记录
tools/                  download/ 环境配置，build/ 编译，flymcu/ 烧录
build/                  自动生成的固件与构建缓存，不入库
CMakeLists.txt          唯一生效的 CMake 构建入口
```

已移除 OLED 驱动与页面、DataScope 上报、校准向导、菜单/PID 按键调参及单双击/长按功能。仅保留启动和软件急停；运行时 LED 闪烁，停机时熄灭。

## 控制流程

```
TIM1 每 20 ms 溢出 → 中断只置 tickPending
main 超级循环 → controlLoop()
  ├─ checkEmergencyStop()  每轮检查 PA2，按下即清零 PWM 并保持停机
  ├─ tickPending          没有 20 ms 节拍则返回
  ├─ buttons.scan(state)  PA5 启动键消抖，急停优先
  ├─ sample()          编码器 / 角度 ADC / 角速度
  ├─ pendulum.tick()   手动平衡 或 自动起摆
  └─ 输出              停止 → 占空比 0；运行 → 限幅后写 TIM3->CCR4
```

角度环是 PD：`u = -(Kp·(θ-3100) + Kd·dθ)`，初始 `Kp=Kd=400`；
位置环每 120 ms 一次 PD，初始 `Kp=20, Kd=300`；两环相减后限幅 ±6900（满幅 7200）。

当前 TIM1 控制周期为 **20 ms（50 Hz）**：`Platform/tim.c` 的预分频为 7199、周期为 199，`WHEELTEC.ioc` 与 `App/Control/Config.hpp` 已同步。控制参数和控制环分频计数保留原值，角速度差分每 120 ms 更新一次。PID 参数原先用于 5 ms 节拍，20 ms 下的平衡效果尚未进行硬件验证。

根目录只保留构建入口、硬件配置和说明。旧 CMake 模板、修复前备份、Keil 工程和旧 IDE 元数据已移出工程；构建不依赖它们。`build/` 可通过 `.\wt -Clean` 清空重建。

## 启动与软件急停

- 上电默认停机。PA5 启动键先松开，再按下并连续采样两次确认后启动；运行中再次按它不会切换或停止。
- PA2 软件急停按下后清零 PWM、保持停机，不等待控制节拍或消抖。两键同时按时急停优先。
- 松开急停不会自动恢复；即使 PA5 一直按着也不会恢复，必须先松开 PA5 并消抖确认，再重新按下启动。
- 角度、电压等已有保护继续生效。保护停机后按住启动键不会反复重启。
- 当前默认手动平衡（`App/Control/ControlState.hpp` 中 `autoRun=false`），模式不再通过按键切换。

这是软件轮询急停，响应仍受主循环其他操作耗时影响，不等同于独立硬件断电急停。

## 串口 1 DMA 打印

在主循环中调用（不要放进中断）：

```cpp
app::Control::instance().uart1Printf("encoder=%d\r\n", 7925);
```

`uart1Printf()` 先格式化到 `Control` 的持久缓冲区，再调用 `HAL_UART_Transmit_DMA()`，启动后立即返回，不等待线上发送完成。最多 127 字节；成功返回已提交字节数，忙、超长、格式化失败或启动失败返回 `-1`。单缓冲不排队：上一条未完成时本次不会覆盖数据，可在后续主循环中重试。空串返回 0。当前 nano printf 未启用 `%f`。

`MX_DMA_Init()` 必须先于 `MX_USART1_UART_Init()`。DMA1 Channel4 中断处理 DMA 传输，USART1 中断处理最后一字节的发送完成，HAL 随后恢复 READY，允许下一次打印。两者抢占优先级为 2，低于 TIM1 的 1。相关通道、模式与 NVIC 配置已写入 `.ioc`，尚未在 CubeMX 图形界面或硬件上验证。

## 构建（新电脑一键配置）

Windows 10/11 x64 上打开 PowerShell，进入工程目录：

```powershell
.\tools\download\setup-toolchain.cmd            # 下载、校验、解压工具，然后完成 Debug 构建
# 或直接使用原来的入口，缺少本项目工具链时会自动配置：
.\wt
```

不需要管理员权限、Python、Conda、Visual Studio 或预先安装 CMake。首次需要联网下载约 360 MB；后续复用用户目录中的便携工具和下载缓存。固定使用 CMake 3.31.10、Arm GNU 12.2.Rel1（包含 GCC/G++ 和 binutils）、xPack Windows GNU Make 4.4.1-3。GNU Make 负责构建，MCU 固件由 Arm GCC/G++ 编译。

```powershell
.\wt -Mode Release              # Release 构建
.\wt -Clean                     # 清空 build 后重建
.\wt -Flash                     # 构建成功后启动 FlyMcu
.\tools\download\setup-toolchain.cmd -InstallOnly  # 只配置环境
.\tools\download\setup-toolchain.cmd -ToolsDir D:\WheeltecTool  # 指定 ASCII 工具目录并构建
```

工具默认放在 `%LOCALAPPDATA%\WheeltecTool`，仅在构建进程里设置 PATH，不改系统或用户 PATH。工程可位于中文目录，脚本自动使用 ASCII 目录联接构建。移动工程或更换工具路径时会检查并重建旧 CMake 缓存。详细版本、缓存和网络说明见 [工具链与交叉编译配置.md](docs/工具链与交叉编译配置.md)。

## 烧录

FlyMcu 串口 ISP（仓库已附带 `tools/flymcu/FlyMcu.exe`）：

1. 板子 `BOOT0=1`、`BOOT1=0`，复位进入 ISP；
2. 选 COM 口、115200，载入 `build\WHEELTEC.hex`，勾选“编程后执行”；
3. 烧完 `BOOT0` 拨回 0，复位运行。

## 文档

- [工具链与交叉编译配置.md](docs/工具链与交叉编译配置.md)：工具链、五个必须遵守的约束、调试建议
- [交接.md](docs/交接.md)：厂家资料与硬件外设的原始交接记录
