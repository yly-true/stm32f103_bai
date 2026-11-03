/**
 * @file    main.cpp
 * @brief   程序入口：只做两件事 —— 初始化所有外设，然后跑超级循环
 *
 *   main()
 *     ├─ HAL_Init / 时钟 / JTAG
 *     ├─ 外设初始化（GPIO、TIM3 PWM、TIM4 编码器、USART1、ADC1/2、TIM1 节拍、OLED）
 *     └─ for (;;) app::Control::instance().controlLoop();
 *
 * 所有控制逻辑、模式切换、显示刷新都在 app::Control 内部，
 * 中断回调只负责置标志（见 App/Control.cpp），因此这里看不到任何业务代码。
 */
#include "main.h"

#include "App/Control.hpp"

namespace {

void JTAG_Set(uint8_t mode)
{
    uint32_t temp = mode;
    temp <<= 25;
    RCC->APB2ENR |= 1u << 0;
    AFIO->MAPR &= 0XF8FFFFFFu;
    AFIO->MAPR |= temp;
}

void SystemClock_Config()
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;                 // 8 MHz × 9 = 72 MHz
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }

    periph.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph.AdcClockSelection = RCC_ADCPCLK2_DIV6;  // ADC 12 MHz
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        Error_Handler();
    }
}

} // namespace

extern "C" void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
        // 停在这里，等待看门狗或手动复位
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    JTAG_Set(JTAG_SWD_DISABLE);   // 关 JTAG，PB3/PB4 才能当 OLED 的 I/O
    JTAG_Set(SWD_ENABLE);         // 保留 SWD 调试口
    HAL_Delay(1000);              // 等电源与传感器稳定
    HAL_Delay(1000);

    MX_GPIO_Init();
    MX_TIM3_Init();               // PWM 10 kHz（TIM3_CH4）
    MX_TIM4_Init();               // 编码器（TIM4，CNT 预置 10000）
    MX_USART1_UART_Init();        // 上位机 128000
    MX_ADC_Init();                // ADC1 角度 / ADC2 电池
    MX_TIM1_Init();               // 5 ms 节拍：只置标志，不开业务
    OLED_Init();

    for (;;) {
        app::Control::instance().controlLoop();   // 到 5 ms 才真正干活，否则立即返回
    }
}
