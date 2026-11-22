#define PAD_PROBE_ONLY
#include "../hand-capture-20261106/capture.c"
#include "swing_logic.h"
#include "state_capture.h"

static AngleWatch watch;
static Hybrid hybrid;
static uint32_t hybrid_next;
static int emitted_stage;
static int emitted_target;
static int emitted_settled;
#ifdef SOLO_ENTRY_DIAGNOSTIC
static uint16_t angle_burst[10];
static unsigned burst_attempts;
static uint32_t last_burst_report;
static int brake_applied,emitted_brake;
#endif

static int capture_feedback_active(void) {
#ifdef SOLO_ENTRY_DIAGNOSTIC
    if (hybrid.stage==HYB_MOVING_CAPTURE) return 1;
#endif
    return hybrid.stage==HYB_BALANCE;
}

static void checked_sample(int *minimum,int *maximum) {
    unsigned sum=0;
    *minimum=4095; *maximum=0;
    for (unsigned i=0;i<10;++i) {
        int value=Get_Adc(3);
#ifdef SOLO_ENTRY_DIAGNOSTIC
        angle_burst[i]=(uint16_t)value;
#endif
        sum+=(unsigned)value;
        if (value<*minimum) *minimum=value;
        if (value>*maximum) *maximum=value;
    }
    raw_angle=(int)(sum/10);
    voltage=Get_Adc2(Battery_Ch)*3630/4096;
    extern ADC_HandleTypeDef hadc2;
    if (hadc1.ErrorCode!=HAL_ADC_ERROR_NONE) raw_angle=*minimum=*maximum=-1;
    if (hadc2.ErrorCode!=HAL_ADC_ERROR_NONE) voltage=0;
}

#ifdef SOLO_ENTRY_DIAGNOSTIC
static int confirm_scattered_burst(int *minimum,int *maximum) {
    if (!capture_feedback_active() || !watch.rate_ready || *maximum-*minimum<=128 ||
        raw_angle<64 || raw_angle>4031 || abs_i(raw_angle-watch.last)>512) return NONE;
    // Disarm the timer during acquisition. The next duty must use the confirmed reading.
    coast(); direction=command=0;
    uint16_t first[10];
    int first_mean=raw_angle;
    for (unsigned i=0;i<10;++i) first[i]=angle_burst[i];
    checked_sample(minimum,maximum);
    int good=confirmed_angle_burst(first,watch.last,raw_angle,*minimum,*maximum);
    ++burst_attempts;
    uint32_t now=micros();
    if (burst_attempts==1 || !good || now-last_burst_report>=250000U) {
        int record[16]={(int)count,(int)burst_attempts,good,watch.last,first_mean,raw_angle};
        for (unsigned i=0;i<5;++i) {
            record[6+i]=first[i*2]|((int)first[i*2+1]<<16);
            record[11+i]=angle_burst[i*2]|((int)angle_burst[i*2+1]<<16);
        }
        if (!stream_frame(9,record,16)) return good ? TRACE_END : SENSOR_END;
        last_burst_report=now;
    }
    return good ? NONE : SENSOR_END;
}
#endif

static int basic_guard(uint32_t now,int enc) {
    if (stop_key()) return STOP_KEY_END;
    if (!power_ok(voltage)) return VOLTAGE_END;
#ifdef HAND_STATE_DIAGNOSTIC
    if (abs_i(enc-capture.origin)>=1000) return TRAVEL_END;
    if (hybrid.stage==HYB_BALANCE && now-capture.start-hybrid.balance_at>=5000000U) return TIME_END;
#endif
    if (abs_i(enc)>=HYB_HOME_COUNTS || abs_i(enc-capture.origin)>=HYB_HOME_COUNTS) return TRAVEL_END;
    if (now-capture.start>=LENGTH_US) return TIME_END;
    if (now-last_sample>MAX_INTERVAL_US) return TIMING_END;
    return NONE;
}

static void hybrid_control(uint32_t now,int enc,int minimum,int maximum) {
    int reason=hybrid_step(&hybrid,&watch,now-capture.start,enc,raw_angle);
    if (reason) { finish(now,enc,reason); return; }
    int angle_part=0,position_part=0;
    int request=hybrid.request;
    if (capture_feedback_active()) {
        request=state_capture_pwm(raw_angle,enc-hybrid.control_origin,hybrid.two_step_delta,
                                  watch.omega,&angle_part,&position_part);
    }
    int hold_limit=HYB_SWING_PWM_MAX;
#ifdef LIMITED_HOLD
    if (hybrid.stage==HYB_BALANCE)
        hold_limit=hold_pwm_limit(now-capture.start-hybrid.balance_at,raw_angle-ZERO_ADC,watch.omega,hybrid.two_step_delta);
#endif
    command=hlimit(request,hold_limit);
#ifdef MERGED_HOLD
    command=paired_motor_output(&hybrid,hold_limit<HYB_SWING_PWM_MAX,command);
#endif
#ifdef GENTLE_HOLD
    if (hybrid.stage==HYB_BALANCE)
        command=quiet_motor_output(&hybrid,now-capture.start-hybrid.balance_at,raw_angle-ZERO_ADC,watch.omega,command);
#endif
    int current_target=capture_feedback_active() ? hybrid.control_origin : hybrid.target;
    int changed=hybrid.stage!=emitted_stage || current_target!=emitted_target || hybrid.settled!=emitted_settled;
#ifdef SOLO_ENTRY_DIAGNOSTIC
    changed|=hybrid.rail_brake!=emitted_brake;
#endif
    if (count%5==0 || changed) {
        int flags=watch.valid | (watch.rate_ready<<1) | (watch.locked<<2) |
                  ((abs_i(raw_angle-HYB_DOWN_ADC)<=600)<<3) | ((abs_i(raw_angle-ZERO_ADC)<=17)<<4) | (hybrid.near_top<<5) |
                  ((hybrid.stage==HYB_BALANCE)<<6) |
                  (hybrid.settled<<7);
#ifdef SOLO_ENTRY_DIAGNOSTIC
        flags|=(hybrid.rail_brake!=0)<<8;
#ifdef TRIM_PUMP_STROKE
        flags|=hybrid.trimmed_stroke<<11;
#endif
#ifdef LIMITED_HOLD
        flags|=(hold_limit<HYB_SWING_PWM_MAX)<<10;
#endif
#ifdef GENTLE_HOLD
        flags|=hybrid.hold_coasting<<9;
#endif
        emitted_brake=hybrid.rail_brake;
#endif
        int state[]={(int)count,hybrid.stage,(int)(watch.omega*1000),(int)(hybrid.energy*1000),flags,maximum-minimum};
        if (!stream_frame(6,state,6)) { finish(now,enc,TRACE_END); return; }
        int trajectory[]={(int)count,current_target,capture_feedback_active() ? (hybrid.settled ? HYB_SETTLED_TRAVEL : HYB_HOME_COUNTS) : hybrid.amplitude,
                          capture_feedback_active() ? 0 : hybrid.speed};
        if (!stream_frame(7,trajectory,4)) { finish(now,enc,TRACE_END); return; }
        emitted_stage=hybrid.stage;
        emitted_target=current_target; emitted_settled=hybrid.settled;
    }
    int values[]={(int)count,(int)(now-capture.start),raw_angle,enc,request,command,(int)cycles,angle_part,position_part};
    if (!stream_frame(2,values,9)) { finish(now,enc,TRACE_END); return; }
    ++count;
    last_sample=now;
}

void ram_start(void) {
    __disable_irq();
    for (uint32_t *v=&__bss_start;v<&__bss_end;++v) *v=0;
    hardware_init(); set_clock_and_adc();
    rig_short_brake();
    uint32_t brake_test=micros();
    while (micros()-brake_test<10U) { }
    if ((GPIOB->IDR&RIG_DIR_MASK)!=RIG_DIR_MASK || (GPIOB->IDR&(1U<<1))) failed();
    int brake_pads[]={(int)(micros()/1000U),-1,(int)(GPIOB->IDR&RIG_DIR_MASK),0,0,(int)TIM3->CCER};
    if (!stream_frame(8,brake_pads,6)) failed();
    coast(); gpio_config(GPIOB,1,10);
    GPIOA->BSRR=LED_Pin; gpio_config(GPIOA,4,2);
    uint32_t idle_next=0,last_report=(uint32_t)-100000;
    for (;;) {
        serial_pump();
        uint32_t now=micros();
        if (capture.phase==RUNNING) {
            int enc=encoder();
            int reason=basic_guard(now,enc);
            if (reason) { finish(now,enc,reason); continue; }
            if ((int32_t)(now-hybrid_next)>=0) {
                int minimum,maximum;
                checked_sample(&minimum,&maximum); now=micros(); enc=encoder();
                reason=basic_guard(now,enc);
                if (reason) { finish(now,enc,reason); continue; }
#ifdef SOLO_ENTRY_DIAGNOSTIC
                int sample_reason=confirm_scattered_burst(&minimum,&maximum);
                now=micros(); enc=encoder();
                reason=basic_guard(now,enc);
                if (reason || sample_reason) { finish(now,enc,reason ? reason : sample_reason); continue; }
#endif
                angle_watch(&watch,raw_angle,minimum,maximum);
                hybrid_control(now,enc,minimum,maximum);
                if (capture.phase!=RUNNING) continue;
                hybrid_next+=PERIOD_US;
            }
            lamp_mode=capture_feedback_active() ? PRESS_READY : LAMP_OFF;
            if (lamp_is_on(lamp_mode,now)) GPIOA->BRR=LED_Pin;
            else GPIOA->BSRR=LED_Pin;
#ifdef SOLO_ENTRY_DIAGNOSTIC
            if (hybrid.rail_brake) {
                if (!brake_applied) {
                    command=direction=0;
                    rig_short_brake();
                    uint32_t changed=micros();
                    while ((GPIOB->IDR&RIG_DIR_MASK)!=RIG_DIR_MASK || (GPIOB->IDR&(1U<<1))) {
                        if (micros()-changed>=10U) { finish(micros(),encoder(),DIRECTION_PAD_END); break; }
                    }
                    brake_applied=1;
                }
                continue;
            }
            if (brake_applied) {
                coast(); gpio_config(GPIOB,1,10); direction=0; brake_applied=0;
            }
#endif
            drive(micros());
            continue;
        }
        if (capture.phase==LOCKED && !(voltage>=450 && voltage<=600)) rig_short_brake();
        else coast();
        if ((int32_t)(now-idle_next)<0) continue;
        int minimum,maximum;
        checked_sample(&minimum,&maximum); now=micros();
        idle_next=now+PERIOD_US;
        angle_watch(&watch,raw_angle,minimum,maximum);
#ifdef HAND_STATE_DIAGNOSTIC
        approach_start(&capture,&watch,now,encoder(),raw_angle,voltage,start_key(),stop_key());
#else
        hybrid_start_check(&capture,&watch,now,encoder(),raw_angle,voltage,start_key(),stop_key());
#endif
        if (capture.phase==RUNNING) {
            hybrid=(Hybrid){0}; hybrid.energy=-1;
#ifdef SOLO_ENTRY_DIAGNOSTIC
            hybrid.run_origin=capture.origin;
#endif
#ifdef HAND_STATE_DIAGNOSTIC
            hybrid.stage=HYB_FREE_APPROACH;
#else
            hybrid.stage=HYB_SEED;
#endif
            emitted_stage=emitted_target=emitted_settled=0;
            last_sample=now; hybrid_next=now+PERIOD_US;
            GPIOA->BSRR=LED_Pin;
            hybrid_control(now,encoder(),minimum,maximum);
            continue;
        }
        if (capture.phase==LOCKED) lamp_mode=LAMP_FINISHED;
        else if (voltage>=450 && voltage<=600) lamp_mode=abs_i(raw_angle-HYB_DOWN_ADC)<150 ? USB_IDENTIFY : LAMP_OFF;
        else lamp_mode=capture.pressing==2 ? RELEASE_READY : (capture.ready ? PRESS_READY : LAMP_OFF);
        if (lamp_is_on(lamp_mode,now)) GPIOA->BRR=LED_Pin;
        else GPIOA->BSRR=LED_Pin;
        if (now-last_report>=100000U) { last_report=now; status(now); }
    }
}
