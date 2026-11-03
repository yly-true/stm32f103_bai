# STM32F103 直线倒立摆

WHEELTEC IP570 直线倒立摆的固件工程，基于 STM32F103C8T6，用 **C++ 重构** 过：

- 硬件抽象成 `Led` / `Motor`（含编码器）/ `Adc` 三个类；
- 控制逻辑收进一个 `Control` 类，`main()` 只负责初始化外设 + 超级循环；
- 全系统只有 **TIM1 一个中断**（每 5 ms 置标志），按键改为轮询。

## 硬件

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6，Cortex-M3，72 MHz，64 KB Flash / 20 KB RAM |
| 摆杆角度 | 电位器 → ADC1_IN3（PA3），0~4095，竖直向上约 3100，竖直向下约 1020 |
| 滑块位置 | 增量编码器 → TIM4（PB6/PB7），上电计数预置 10000 |
| 电机 | 直流减速电机 + H 桥：PWM = TIM3_CH4（PB1），方向 = BIN1/BIN2（PB13/PB12） |
| 电池电压 | 分压 → ADC2_IN6（PA6），7.00 V 以下保护停机 |
| 显示 | 0.96" OLED，软件 I²C（PB3/PB4/PB5 + PA15） |
| 上位机 | USART1（PA9/PA10），128000 波特，DataScope 协议 |
| 按键 | 用户键 PA2（单击/双击/长按）、启停 PA5、菜单 PA7、PID± PA11/PA12 |

## 目录

```
App/
  main.cpp              初始化外设 + for(;;) Control::instance().controlLoop()
  Control.{hpp,cpp}     顶层控制类：采样 → 分派 → 输出 → 按键 → 每 50 ms 界面
  Board.hpp             板级对象：Led + Motor + Adc
  Led.hpp / Motor.hpp / Adc.hpp
  Config.hpp            全部常数（阈值、PID 初值、时序）
  State.hpp             运行状态结构体（Control 的私有成员）
  Pid.hpp               AnglePd / PositionPd / IncrementalPi / PrePositionPd
  Balance.{hpp,cpp}     手动平衡：角度 PD（5 ms）+ 位置 PD（25 ms）
  SwingUp.{hpp,cpp}     自动起摆状态机：归中 → 甩杆 → 接管 → 爬 PID
  Keys.{hpp,cpp}        用户键扫描
  Calibration.{hpp,cpp} 角度传感器校准向导
  RateEstimator.hpp     角速度差分 + 滑动平均
  Ui/show.c             OLED 页面 + DataScope 上报
  Ui/DataScope_DP.C     上位机协议
  Ui/app_view.{h,cpp}   C 显示层读取状态的桥

Platform/               CubeMX 生成的外设初始化、中断、启动文件、系统文件
Drivers/                STM32F1 HAL + CMSIS + OLED 驱动
legacy/                 重构前的原版 C 实现（仅作对照，不参与编译）
```

## 控制流程

```
TIM1 每 5 ms 溢出 → 中断只置 tickPending
main 超级循环 → controlLoop()
  ├─ scanKeys()        4 个功能键轮询 + 2 拍消抖
  ├─ sample()          编码器 / 角度 ADC / 角速度
  ├─ dispatch()        手动平衡 或 自动起摆
  ├─ 输出              停止 → 占空比 0；运行 → 限幅后写 TIM3->CCR4
  ├─ keys.tick5ms()    用户键单击/双击/长按
  └─ 每 50 ms          OLED 刷新 + 串口上报
```

角度环是 PD：`u = -(Kp·(θ-3100) + Kd·dθ)`，初始 `Kp=Kd=400`；
位置环每 25 ms 一次 PD，初始 `Kp=20, Kd=300`；两环相减后限幅 ±6900（满幅 7200）。

## 构建

命令行工具 `wt.cmd`（内部调用 `wt.ps1`）：

```powershell
.\wt.cmd                    # Debug 构建
.\wt.cmd -Mode Release      # Release 构建
.\wt.cmd -Clean             # 清空重建
.\wt.cmd -Flash             # 构建后启动 FlyMcu，hex 路径进剪贴板
```

需要 **Arm GNU Toolchain 12.2**（`arm-none-eabi-gcc`）、**CMake 3.29.6+**、**MinGW Make**（`mingw32-make`）。工具会自动补 PATH、处理中文路径（建 ASCII 目录联接）、按构建类型配置 CMake。

手动构建：

```powershell
cmake -S <ASCII 路径> -B <ASCII 路径>\build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build <ASCII 路径>\build --parallel
```

> 注意：本工程的 `.cpp` 交 C 编译器加 `-x c++ -std=gnu++17` 编译，因为本机 CMake 启用 CXX 语言会在探测阶段崩溃（`0xC0000409`）；代码不使用 STL / 异常 / RTTI，链接不依赖 libstdc++。
> 源码目录名含中文时 `mingw32-make` 无法处理，需用 ASCII 目录联接（`wt.ps1` 会自动做）。

## 烧录

FlyMcu 串口 ISP（仓库已附带 `FlyMcu.exe`）：

1. 板子 `BOOT0=1`、`BOOT1=0`，复位进入 ISP；
2. 选 COM 口、115200，载入 `build\WHEELTEC.hex`，勾选“编程后执行”；
3. 烧完 `BOOT0` 拨回 0，复位运行。

## 文档

- [工具链与交叉编译配置.md](工具链与交叉编译配置.md)：工具链、五个必须遵守的约束、调试建议
- [交接.md](交接.md)：厂家资料与硬件外设的原始交接记录
