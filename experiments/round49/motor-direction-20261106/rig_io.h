#pragma once
#include "stm32f103xb.h"

// Positive is the WHEELTEC convention (BIN1 high, BIN2 low). Each independently
// rearmed 100 us timer period emits only a 12.5 us high tail and stops in hardware.
enum { RIG_BIN1_MASK = 1U << 13, RIG_BIN2_MASK = 1U << 12,
       RIG_DIR_MASK = RIG_BIN1_MASK | RIG_BIN2_MASK, START_KEY = 1U << 5, STOP_KEY = 1U << 2 };
extern uint32_t __bss_start, __bss_end, __stack_top;
static uint16_t last_timer;
static uint32_t elapsed;

static void coast(void) {
    GPIOB->BRR = RIG_DIR_MASK;
    TIM3->CCER = 0;
    TIM3->CR1 = TIM_CR1_OPM;
    TIM3->CNT = 0;
}

static void rig_short_brake(void) {
    coast();
    GPIOB->BRR=1U<<1;
    GPIOB->CRL=(GPIOB->CRL&~(15U<<4))|(2U<<4);
    GPIOB->CRH=(GPIOB->CRH&~(255U<<16))|(0x22U<<16);
    GPIOB->BSRR=RIG_DIR_MASK; // TB6612 IN1=IN2=H: both motor terminals low.
}

static inline unsigned rig_direction_mask(int sign) {
    return sign > 0 ? RIG_BIN1_MASK : RIG_BIN2_MASK;
}

// Call only after coast() and with nonzero sign.
static inline void rig_select_direction(int sign) {
    GPIOB->BSRR = rig_direction_mask(sign);
}

static void failed(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
#ifdef HYBRID_TRIAL
    rig_short_brake();
#else
    coast();
#endif
    for (;;) { }
}

static uint32_t micros(void) {
    uint16_t current = (uint16_t)TIM2->CNT;
    elapsed += (uint16_t)(current - last_timer);
    last_timer = current;
    return elapsed;
}

static void gpio_config(GPIO_TypeDef *port, unsigned pin, unsigned nibble) {
    volatile uint32_t *reg = pin < 8 ? &port->CRL : &port->CRH;
    unsigned shift = (pin % 8) * 4;
    *reg = (*reg & ~(15U << shift)) | (nibble << shift);
}

static void hardware_init(void) {
    __disable_irq();
    SysTick->CTRL = 0;
    SCB->VTOR = 0x20001000;
    __DSB(); __ISB();
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { }
    RCC->CFGR &= ~RCC_CFGR_SW;
    while (RCC->CFGR & RCC_CFGR_SWS) { }
    RCC->CFGR = 0; // HSI 8 MHz, APB1/APB2 8 MHz, ADC 4 MHz
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN |
                     RCC_APB2ENR_ADC1EN | RCC_APB2ENR_USART1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM4EN;
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST | RCC_APB1RSTR_TIM3RST | RCC_APB1RSTR_TIM4RST;
    RCC->APB1RSTR &= ~(RCC_APB1RSTR_TIM2RST | RCC_APB1RSTR_TIM3RST | RCC_APB1RSTR_TIM4RST);
    RCC->APB2RSTR |= RCC_APB2RSTR_ADC1RST | RCC_APB2RSTR_USART1RST;
    RCC->APB2RSTR &= ~(RCC_APB2RSTR_ADC1RST | RCC_APB2RSTR_USART1RST);
    coast();
    gpio_config(GPIOB, 12, 2); gpio_config(GPIOB, 13, 2);
    GPIOA->BSRR = START_KEY | STOP_KEY;
    gpio_config(GPIOA, 2, 8); gpio_config(GPIOA, 5, 8);
    gpio_config(GPIOA, 3, 0); gpio_config(GPIOA, 6, 0);
    gpio_config(GPIOB, 6, 4); gpio_config(GPIOB, 7, 4);
    gpio_config(GPIOA, 9, 11);
    USART1->BRR = 69; // 115200 8N1, transmit only
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
    TIM2->PSC = 7; TIM2->ARR = 65535; TIM2->EGR = TIM_EGR_UG; TIM2->CR1 = TIM_CR1_CEN;
    TIM4->ARR = 65535;
    TIM4->CCMR1 = 0xa1a1; // timer inputs 1 and 2, filter 10 as in the WHEELTEC firmware
    TIM4->SMCR = 3; TIM4->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
    TIM4->EGR = TIM_EGR_UG; TIM4->CNT = 0; TIM4->CR1 = TIM_CR1_CEN;
    TIM3->PSC = 0; TIM3->ARR = 799; TIM3->CCR4 = 700;
    TIM3->CCMR2 = 7U << 12; // output compare CH4, PWM2: low at CNT=0, high at 700..799
    TIM3->EGR = TIM_EGR_UG;
    gpio_config(GPIOB, 1, 10);
    // Local hardware self-test with both H-bridge directions low and CC4 disabled.
    TIM3->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
    uint32_t started = micros();
    while (TIM3->CR1 & TIM_CR1_CEN) if (micros() - started > 200U) failed();
    if (micros() - started < 80U || TIM3->CNT != 0) failed();
    ADC1->CR2 = ADC_CR2_ADON;
    started = micros(); while (micros() - started < 10U) { }
    ADC1->CR2 |= ADC_CR2_RSTCAL; while (ADC1->CR2 & ADC_CR2_RSTCAL) { }
    ADC1->CR2 |= ADC_CR2_CAL; while (ADC1->CR2 & ADC_CR2_CAL) { }
    ADC1->SMPR2 = (7U << 9) | (7U << 18);
    ADC1->CR2 |= ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG;
}

static int sample_adc(unsigned channel) {
    ADC1->SQR3 = channel;
    ADC1->SR = 0;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    uint32_t start = micros();
    while (!(ADC1->SR & ADC_SR_EOC)) if (micros() - start > 300U) failed();
    return (int)ADC1->DR;
}

static void put(char c) {
    while (!(USART1->SR & USART_SR_TXE)) { }
    USART1->DR = (unsigned char)c;
}

static void number(int value) {
    char chars[12]; unsigned n = 0;
    unsigned v = value < 0 ? (unsigned)(-value) : (unsigned)value;
    if (value < 0) put('-');
    do { chars[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) put(chars[--n]);
}
