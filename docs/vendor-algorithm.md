# 官方原版（原本的版本）算法与数值对照

来源：`WHEELTEC IP570 倒立摆STM32源码/1.直线倒立摆STM32源码/原本的版本`
关键文件：`BALANCE/CONTROL/control.c`、`Core/Src/main.c`、`Core/Src/tim.c`、`HARDWARE/ADC/adc.c`、`HARDWARE/ENCODER/encoder.c`

## 0. 运行框架

| 项目 | 官方数值 |
|---|---|
| 控制节拍 | TIM1：PSC=7199，ARR=49 → 72 MHz / 7200 / 50 = **200 Hz（5 ms）** |
| 控制入口 | `HAL_TIM_PeriodElapsedCallback(htim1)` 里做全部控制 |
| 采样 | `Encoder = Read_Encoder(4)`；`Angle_Balance = Get_Adc_Average(3, 10)` |
| 启动 | `Flag_Stop = 1`（停机），按键翻转；`Flag_Stop==0` 才出力 |
| PWM | TIM3_CH4，ARR=7199（满幅 7200），限幅 **±6900** |
| 电机方向 | `moto<0 → BIN2=1,BIN1=0`；否则 `BIN2=0,BIN1=1` |
| 电压 | `Get_Adc(6) × 3.3 × 11 × 100 / 4096`（mV） |

## 1. 角度环（每 5 ms）

```c
Bias    = Angle_Balance - 3100;          // 3100 = 竖直向上
D_Bias  = Bias - Last_Bias;              // 5 ms 差分，未除以 dt
balance = -Balance_KP * Bias - Balance_KD * D_Bias;
Last_Bias = Bias;
```

`Balance_KP = 400`，`Balance_KD = 400`（`Core/Src/main.c`）。

量纲：`Bias` 是 **ADC 码**，1 码 = 180° / (3100-1020) = **0.08654°**。

换算成物理单位：

| | 官方（码） | 等效（度） | 本工程（度） |
|---|---|---|---|
| Kp | 400 | 400 × 11.556 = **4622 /度** | 400 × 11.556 = 4622 |
| Kd | 400 | 400 × 0.005 × 11.556 = **23.1 /(度/秒)** | 400 × 0.005 × 11.556 = 23.1 |

两者完全等价。

## 2. 位置环（每 5 个节拍 = 25 ms）

```c
Position_Least      = Encoder - Position_Zero;
Position_Bias       = Position_Bias * 0.8 + Position_Least * 0.2;   // 一阶低通
Position_Differential = Position_Bias - Last_Position;
Last_Position       = Position_Bias;
Position_PWM        = Position_Bias * Position_KP + Position_Differential * Position_KD;
```

`Position_KP = 20`，`Position_KD = 300`。

- 低通 α=0.2、周期 25 ms → τ = −0.025 / ln(0.8) = **0.112 s**
- 微分项按 25 ms 差分，未除以 dt
- 等效微分增益：300 × 0.025 = **7.5 PWM/(码/秒)**

## 3. 输出融合

```c
Moto = Balance_Pwm - Position_Pwm;
Xianfu_Pwm();          // ±6900
Set_Pwm(Moto);
```

注意：官方是 **减**。它隐含的约定是「编码器增大的方向 = +PWM 使车运动的方向」。若编码器 A/B 相反，这一项就要改成加。

## 4. 保护

```c
if (!(Angle_Balance > 2600 && Angle_Balance < 3600) || voltage < 700)
    Flag_Stop = 1;      // |角度偏差| > 500 码 = 43.3°，或电压 < 7.00 V
```

起摆模式额外的边界：`Encoder >= 9900 || Encoder <= 5900` → 停机。

## 5. 角速度估计（每 6 个节拍 = 30 ms）

```c
if (++D_Count > 5) {                       // 6 × 5 ms = 30 ms
    D_Angle_Balance = Mean_Filter(Angle_Balance - Last_Angle_Balance);
    Last_Angle_Balance = Angle_Balance;
    D_Count = 0;
}
// Mean_Filter：4 点滑动平均（FILTERING_TIMES = 4）
```

## 6. 自动起摆

### 6.1 Find_Zero（回中）

```c
Position_Zero = 7925;                            // POSITION_MIDDLE
Balance_Pwm   = Balance(Angle_Balance + 2070) / 8;   // 把"竖直向下"折算到竖直向上
Position_Pwm  = Pre_Position(Encoder);               // Kp=25, Kd=600
if (Encoder < 7950) Moto = Incremental_PI(Encoder, 7925);  // Kp=0.5, Ki=0.5
else                Moto = -Balance_Pwm + Position_Pwm;
限幅 ±2500;
```

稳定判据：`|Angle-1020| < 300` 且 `|Encoder-7925| < 50`，每拍 `count++`（另在
`|Angle-1020| < 300` 时 `+0.1`），`count > 200` 后 `autorun_step0=1`、
`Target_Position = 7925 - 668 = 7257`。

### 6.2 Auto_run（甩杆）

```c
方向判定：|Angle-1020| < 120 时，D_Angle <= 0 → right=1，否则 left=1
left  → Target_Position = 7925 + 800 = 8725；speed>1 时改用 +160
right → Target_Position = 7925 - 482 = 7443；speed>1 时改用 -160
Moto = Position_PID(Encoder, Target_Position);      // Kp=20, Kd=300
限幅 +4100 / -5100
```

起摆成功判据（连续 3 次）：
`|Angle-3100| < 120` 且 `6300 < Encoder < 9300` 且 `|D_Angle| < 30`。

成功后切换到「软」参数：

```c
Balance_KP = 210, Balance_KD = 150, Position_KP = 8, Position_KD = 130;
```

### 6.3 起摆后接管

```c
if (wait_count == 0) Position_Zero = Encoder;
// 每 5 拍跑一次位置环，Moto = Balance_Pwm - Position_Pwm
wait_count++;
if (wait_count > 100 && wait_count < 2000) {  // 缓慢把目标挪回 8100
    if (Position_Zero > 8100) Position_Zero--;
    else if (Position_Zero < 8100) Position_Zero++;
}
// 每 100 拍把 PID 往默认值爬一档：+10 / +10 / +0.5 / +10
```

## 7. 按键与编码器分辨率

```c
int position = 1040;   // 13 线霍尔 × 4 倍频 × 20 减速比 = 1040 码 / 电机轴一圈
```

单击 ±4 码地改 `Position_Zero`，改满 1040 码（电机一圈）停止。
长按 KEY2：`Position_Zero = 10000` 并切换 `auto_run`。

`Read_Encoder(4)` 返回 `(short)TIM4->CNT`，上电时 `TIM_SetCounter(TIM4, 10000)`。

## 8. 与本工程的逐项对照

| 项目 | 官方 | 本工程 | 评价 |
|---|---|---|---|
| 节拍 | 5 ms（ARR=49） | 5 ms（ARR=49） | 一致 |
| 角度单位 | ADC 码 | 度 | 已换算，等价 |
| 角度 Kp | 400 /码 | 4622 /度 | 等价 |
| 角度 Kd | 400 /(码/5ms) | 23.1 /(度/秒) | 等价 |
| 位置周期 | 25 ms（每 5 拍） | 30 ms（每 6 拍） | 略慢 |
| 位置低通 α | 0.2（τ=0.112 s） | 0.5（τ=0.03 s） | 更快，少相位滞后 |
| 位置 Kp | 20 /码 | 5 /码 | 1/4 |
| 位置 Kd | 300 /(码/25ms) ≈ 7.5 /(码/秒) | 3.0 /(码/秒) | 约 1/2.5 |
| 位置限幅 | 无（靠 PD 自然收敛） | 等效 3° 倾角 | 新增，防推倒 |
| 输出融合 | Balance − Position | Balance + Position | 符号相反，见下 |
| 角度保护 | \|偏差\| > 500 码（43°） | 进入 26° / 退出 52° | 滞回，更严 |
| 静差 | 无积分 | 无积分 | 相同 |
| 起摆 | 完整状态机 | 未启用（autoRun=false） | 未移植 |

## 9. 关于输出符号相反

官方 `Moto = Balance_Pwm - Position_Pwm` 能工作，隐含「编码器增大 = +PWM 的运动方向」。
本机实测（VOFA+ 90 s 数据，`|Moto|>2000` 样本）：`Moto>0` 时编码器下一拍下降 97%，
`Moto<0` 时上升 98%，即 **+PWM 使编码器减小**，方向相反。

角度环的符号没有改动且能稳杆，说明电机与角度传感器方向都与官方一致；
差别只在**编码器计数方向**（A/B 相接线相反）。因此：

- 位置项必须改成加号，本工程已改
- 官方所有以编码器码表示的常数（5900 / 7925 / 9900 / 6300 / 9300）在本机上
  应镜像理解为 `20000 - 官方值`（上电预置同为 10000 时）
- 若之后移植起摆逻辑，`left/right` 判据和边界保护都要一并按镜像处理

## 10. 可用于标定的官方线索

`13 线 × 4 倍频 × 20 减速比 = 1040 码 / 电机输出轴一圈`。

若已知同步带轮齿数与齿距（如 GT2 20 齿 = 40 mm/圈），则

```
kMetersPerCount = 带轮每圈行程 / 1040
例：0.040 m / 1040 = 3.85e-5 m/count ≈ 0.0385 mm/count
```

本工程目前用 0.1 mm/count 是占位值，偏大约 2.6 倍；改用上式可得到真实米制尺度。
