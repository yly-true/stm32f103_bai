#include "rig_io.h"
#include "adc.h"
#include "capture_limits.h"
#ifdef STREAM_TRIAL
#include "telemetry_stream.h"
static void serial_pump(void) {
    uint8_t byte;
    if ((USART1->SR&USART_SR_TXE) && stream_pop(&byte)) USART1->DR=byte;
}
static void serial_flush(void) {
    while (stream_head!=stream_tail) { serial_pump(); (void)micros(); }
    while (!(USART1->SR&USART_SR_TC)) { (void)micros(); }
}
#endif
_Static_assert(RIG_BIN1_MASK == BIN1_Pin && RIG_BIN2_MASK == BIN2_Pin,
               "use GPIO pin masks, never the vendor's bit-band output aliases");

extern void reset_pd(void);
extern int angle_pwm(int raw);
#ifdef POSITION_TRIAL
extern int guarded_position_pwm(int raw, int relative_encoder, int *angle_part, int *position_part);
#endif
uint32_t HAL_GetTick(void) { return micros()/1000U; }
void Error_Handler(void) { failed(); }

typedef struct { uint32_t us; int adc, enc, req, cmd, cycles;
#ifdef POSITION_TRIAL
    int angle_req, position_req;
#endif
} Row;
#ifndef STREAM_TRIAL
static Row rows[TRACE_CAP];
#endif
static Capture capture;
static unsigned count, cycles;
#ifdef RELEASE_TRIAL
static int lamp_mode;
#endif
static int raw_angle, voltage, command, direction;
static uint32_t last_sample;
#ifndef PAD_PROBE_ONLY
static uint32_t next_sample;
#endif

static int encoder(void) { return (int16_t)TIM4->CNT; }
static int start_key(void) { return !(GPIOA->IDR & START_KEY); }
static int stop_key(void) { return !(GPIOA->IDR & STOP_KEY); }

// Match the checked-in application's HSE, ADC clock, dual converters and angle
// averaging. PA3 and PA6 are sampled by the actual unmodified Platform/adc.c.
static void set_clock_and_adc(void) {
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { }
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { }
    RCC->CFGR = RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
    SystemCoreClock = 72000000;
    TIM2->PSC = 71; TIM2->EGR = TIM_EGR_UG; TIM2->CNT = 0;
    last_timer = 0; elapsed = 0;
    TIM3->PSC = 8; TIM3->EGR = TIM_EGR_UG;
    USART1->BRR = 625;
    // A second timer check at the final clock; both direction pins and CC4 are off.
    uint32_t start = micros();
    TIM3->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
    while (TIM3->CR1 & TIM_CR1_CEN) if (micros()-start > 150U) failed();
    if (micros()-start < 95U || TIM3->CNT != 0) failed();
    RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;
    RCC->APB2RSTR |= RCC_APB2RSTR_ADC1RST | RCC_APB2RSTR_ADC2RST;
    RCC->APB2RSTR &= ~(RCC_APB2RSTR_ADC1RST | RCC_APB2RSTR_ADC2RST);
    MX_ADC_Init();
}

static void sample(void) {
    raw_angle = Get_Adc_Average(3,10);
    voltage = Get_Adc2(Battery_Ch) * 3630 / 4096; // same 3.3 V, 11:1 divider, centivolts
    if (hadc1.ErrorCode != HAL_ADC_ERROR_NONE) raw_angle = -1;
    extern ADC_HandleTypeDef hadc2;
    if (hadc2.ErrorCode != HAL_ADC_ERROR_NONE) voltage = 0;
}

static void line(const char *tag, const int *values, unsigned n) {
    while (*tag) put(*tag++);
    for (unsigned i=0; i<n; ++i) { put(','); number(values[i]); }
    put('\r'); put('\n');
}

static void status(uint32_t now) {
    int ready=capture.ready;
#ifdef RELEASE_TRIAL
    if (capture.pressing==2) ready=2;
#endif
    int vals[] = {(int)(now/1000U),capture.phase,ready,start_key()|stop_key()<<1,
                  raw_angle,voltage,encoder(),capture.reason,capture.origin,
                  capture.phase == LOCKED && count ? (int)(capture.finish-capture.start) : 0,
                  (int)count,(int)cycles};
#ifdef STREAM_TRIAL
    if (!stream_frame(1,vals,sizeof(vals)/sizeof(vals[0]))) failed();
#else
    line("BAL1",vals,sizeof(vals)/sizeof(vals[0]));
#endif
#ifdef RELEASE_TRIAL
    int lamp[]={(int)(now/1000U),lamp_mode,(GPIOA->IDR & LED_Pin)==0};
#ifdef STREAM_TRIAL
    if (!stream_frame(4,lamp,3)) failed();
#else
    line("LED1",lamp,3);
#endif
#endif
#ifdef HYBRID_TRIAL
    int pads[]={(int)(now/1000U),capture.phase,(int)(GPIOB->IDR&RIG_DIR_MASK),
                (GPIOB->IDR&(1U<<1))!=0,(TIM3->CR1&TIM_CR1_CEN)!=0,(int)TIM3->CCER};
    if (!stream_frame(8,pads,6)) failed();
#endif
}

static void finish(uint32_t now, int enc, int why) {
#ifdef HYBRID_TRIAL
    rig_short_brake();
#else
    coast();
#endif
#ifdef RELEASE_TRIAL
    GPIOA->BRR=LED_Pin; // already coasted: the operator can recover the rod
    lamp_mode=LAMP_FINISHED;
#endif
    command=direction=0;
    end_capture(&capture, now, enc, why);
#ifndef STREAM_TRIAL
    for (unsigned i=0; i<count; ++i) {
        const Row *r=&rows[i];
        int vals[] = {(int)i,(int)r->us,r->adc,r->enc,r->req,r->cmd,r->cycles};
        line("BALD1",vals,sizeof(vals)/sizeof(vals[0]));
#ifdef POSITION_TRIAL
        int parts[]={(int)i,r->angle_req,r->position_req};
        line("BALP1",parts,sizeof(parts)/sizeof(parts[0]));
#endif
    }
#else
    serial_flush();
#endif
    int end[] = {(int)count,why,(int)(now-capture.start),enc,raw_angle,voltage,(int)cycles};
#ifdef STREAM_TRIAL
    if (!stream_frame(3,end,sizeof(end)/sizeof(end[0]))) failed();
    serial_flush();
#else
    line("BALE1",end,sizeof(end)/sizeof(end[0]));
#endif
    status(micros());
}

static void control_sample(uint32_t now, int enc) {
#ifndef STREAM_TRIAL
    if (count >= TRACE_CAP) { finish(now,enc,TRACE_END); return; }
#endif
#ifdef POSITION_TRIAL
    int angle_part,position_part;
    int request=guarded_position_pwm(raw_angle,enc-capture.origin,&angle_part,&position_part);
#else
    int request = angle_pwm(raw_angle);
#endif
    command = cap_pwm(request);
#ifdef STREAM_TRIAL
    int values[]={(int)count,(int)(now-capture.start),raw_angle,enc,request,command,(int)cycles,angle_part,position_part};
    if (!stream_frame(2,values,sizeof(values)/sizeof(values[0]))) {
        finish(now,enc,TRACE_END); return;
    }
    ++count;
#elif defined(POSITION_TRIAL)
    rows[count++] = (Row){now-capture.start,raw_angle,enc,request,command,(int)cycles,angle_part,position_part};
#else
    rows[count++] = (Row){now-capture.start,raw_angle,enc,request,command,(int)cycles};
#endif
    last_sample = now;
}

// One 100 us hardware one-pulse period at a time. A frozen CPU cannot rearm
// the next period. Change CCR only when the timer has returned to CNT=0.
static int start_pwm_period(unsigned high) {
    if (!high || (TIM3->CR1 & TIM_CR1_CEN)) return 0;
    TIM3->CCR4 = 800U-high;
    TIM3->CCER = TIM_CCER_CC4E;
    TIM3->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
    return 1;
}

static void drive(uint32_t now) {
    int dir = command > 0 ? 1 : (command < 0 ? -1 : 0);
    unsigned high = high_ticks(command);
    if (!high || now-capture.start >= LENGTH_US-105U) dir=0;
    if (dir != direction) {
        coast();
        direction = dir;
        if (dir) {
            rig_select_direction(dir);
            uint32_t changed=micros();
            while ((GPIOB->IDR & RIG_DIR_MASK)!=rig_direction_mask(dir)) {
                if (micros()-changed>=5U) { finish(micros(),encoder(),DIRECTION_PAD_END); return; }
            }
        }
    }
    if (!dir || (TIM3->CR1 & TIM_CR1_CEN)) return;
    if ((GPIOB->IDR & RIG_DIR_MASK)!=rig_direction_mask(dir)) {
        finish(micros(),encoder(),DIRECTION_PAD_END); return;
    }
    if (start_pwm_period(high)) ++cycles;
}

void ram_start(void);
#ifndef PAD_PROBE_ONLY
void ram_start(void) {
    __disable_irq();
    for (uint32_t *v=&__bss_start; v<&__bss_end; ++v) *v=0;
    hardware_init();
    set_clock_and_adc();
#ifdef RELEASE_TRIAL
    GPIOA->BSRR=LED_Pin; gpio_config(GPIOA,4,2);
#endif
    uint32_t next_idle=0, last_report=(uint32_t)-100000;
    for (;;) {
#ifdef STREAM_TRIAL
        serial_pump();
#endif
        uint32_t now=micros();
        if (capture.phase == RUNNING) {
            int pos=encoder();
            int reason=stop_reason(&capture,now,pos,raw_angle,voltage,stop_key(),last_sample);
            if (reason) { finish(now,pos,reason); continue; }
            if ((int32_t)(now-next_sample)>=0) {
                sample();
                now=micros(); pos=encoder();
                reason=stop_reason(&capture,now,pos,raw_angle,voltage,stop_key(),last_sample);
                if (reason) { finish(now,pos,reason); continue; }
                control_sample(now,pos);
                if (capture.phase != RUNNING) continue;
                next_sample+=PERIOD_US;
            }
            drive(micros());
            continue;
        }
        if ((int32_t)(now-next_idle)<0) continue;
        sample(); now=micros();
        next_idle=now+PERIOD_US;
        check_start(&capture,now,encoder(),raw_angle,voltage,start_key(),stop_key());
#ifdef RELEASE_TRIAL
        lamp_mode=release_lamp_mode(&capture,voltage,raw_angle);
        if (lamp_is_on(lamp_mode,now)) GPIOA->BRR=LED_Pin;
        else GPIOA->BSRR=LED_Pin;
#endif
        if (capture.phase == RUNNING) {
            reset_pd();
            next_sample=now+PERIOD_US;
            control_sample(now,capture.origin);
            continue;
        }
        if (now-last_report>=100000U) { last_report=now; status(now); }
    }
}

#endif

__attribute__((section(".vectors"),used))
const uintptr_t vectors[16] = {
    (uintptr_t)&__stack_top,(uintptr_t)ram_start,(uintptr_t)failed,(uintptr_t)failed,
    (uintptr_t)failed,(uintptr_t)failed,(uintptr_t)failed,0,0,0,0,(uintptr_t)failed,
    (uintptr_t)failed,0,(uintptr_t)failed,(uintptr_t)failed
};
