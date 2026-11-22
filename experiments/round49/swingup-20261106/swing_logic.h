#pragma once
#include "../hand-capture-20261106/capture_limits.h"

enum { HYB_SEED=1, HYB_PUMP=2, HYB_BALANCE=3, HYB_FREE_APPROACH=4, SENSOR_END=11, SWING_TIMEOUT_END=12 };
#ifdef SOLO_ENTRY_DIAGNOSTIC
#ifndef SOLO_ENTRY_WINDOW_ADC
#define SOLO_ENTRY_WINDOW_ADC 231
#endif
enum { HYB_MOVING_CAPTURE=5, APPROACH_TIMEOUT_END=13, APPROACH_ANGLE_END=14,
       BRAKING_DISTANCE_END=15, SOLO_ENTRY_ADC=SOLO_ENTRY_WINDOW_ADC, SOLO_INNER_ADC=80, SOLO_APPROACH_US=300000,
       HYB_HOME_COUNTS=1800, HYB_SOFT_COUNTS=1550, HYB_START_COUNTS=400, HYB_SETTLED_TRAVEL=650,
       HYB_BLIND_BRAKE_PROJECTION=1300, HYB_BRAKE_PROJECTION=1650 };
#else
enum { HYB_HOME_COUNTS=1300, HYB_SOFT_COUNTS=1050, HYB_START_COUNTS=80, HYB_SETTLED_TRAVEL=260 };
#endif
enum { HYB_DOWN_ADC=1050, HYB_SWING_PWM_MAX=6900,
       HYB_INITIAL_AMPLITUDE=400, HYB_MAX_AMPLITUDE=900, HYB_INITIAL_SPEED=2000, HYB_MAX_SPEED=5000 };
typedef struct {
    int last;
    uint8_t seen,locked,recovery,n,valid,rate_ready;
    int history[5];
    float omega;
} AngleWatch;
typedef struct {
    int balance_origin,request,previous_encoder,encoder_delta;
    int amplitude,target,speed,switches;
    uint8_t stage,bad_active,have_encoder,departed,near_top,settled,quiet_count;
#ifdef TRIM_PUMP_STROKE
    uint8_t trimmed_stroke;
#endif
    int prior_step, two_step_delta, control_origin;
    uint32_t balance_at,bad_since;
    uint32_t last_switch;
    uint32_t settled_at;
#ifdef SOLO_ENTRY_DIAGNOSTIC
    uint32_t approach_at;
    int approach_error,run_origin,rail_brake;
#ifdef GENTLE_HOLD
    int8_t hold_direction,reverse_count,hold_coasting;
#endif
#ifdef MERGED_HOLD
    int merge_phase,first_request,held_request;
#ifdef CARRY_HOLD
    int pair_remainder;
#endif
#endif
#endif
    float centre_filter,previous_centre,energy;
    float reference;
} Hybrid;
static inline float hfabs(float x) { return x<0 ? -x : x; }
static inline int hsign(float x) { return x>0 ? 1 : (x<0 ? -1 : 0); }
static inline int hlimit(int x,int cap) { return x>cap ? cap : (x<-cap ? -cap : x); }
#ifdef LIMITED_HOLD
static inline int hold_pwm_limit(uint32_t age,int error,float omega,int delta) {
    if (age>=2000000U && abs_i(error)<=32 && hfabs(omega)<=1.25f && abs_i(delta)<=24)
        return 3000;
    return HYB_SWING_PWM_MAX;
}
#endif
#ifdef MERGED_HOLD
// Apply the first eligible request immediately, then average signed pairs.
static inline int paired_motor_output(Hybrid *h,int near,int command) {
    if (!near || !h->merge_phase) {
        h->merge_phase=near ? 1 : 0;
        h->held_request=command;
#ifdef CARRY_HOLD
        h->pair_remainder=0;
#endif
    } else if (h->merge_phase==1) {
        h->first_request=command;
        h->merge_phase=2;
    } else {
        h->held_request=(h->first_request+command)/2;
#ifdef CARRY_HOLD
        int owed=h->held_request+h->pair_remainder;
        h->held_request=abs_i(owed)<600 ? 0 : hlimit(owed,3000);
        h->pair_remainder=owed-h->held_request;
#endif
        h->merge_phase=1;
    }
    return h->held_request;
}
#endif
#ifdef GENTLE_HOLD
// Suppress transient direction reversals only after capture. Pending torque is zero.
static inline int quiet_motor_output(Hybrid *h,uint32_t age,int error,float omega,int cmd) {
    int sign=(cmd>0)-(cmd<0);
    int near=age>=2000000U && abs_i(error)<=32 && hfabs(omega)<=1.25f && abs_i(h->two_step_delta)<=24;
    int reversing=near && sign && h->hold_direction && sign!=h->hold_direction;
    h->reverse_count=reversing ? h->reverse_count+1 : 0;
    h->hold_coasting=reversing && h->reverse_count<3;
    if (h->hold_coasting) return 0;
    if (sign) h->hold_direction=(int8_t)sign;
    return cmd;
}
#endif
static inline int hybrid_capture_kd(uint32_t age) {
    if (age<1000000U) return 400;
    if (age<2000000U) return 400-(int)(300U*(age-1000000U)/1000000U);
    return 100;
}

static inline void angle_watch(AngleWatch *w,int adc,int minimum,int maximum) {
    int delta=w->seen ? adc-w->last : 0;
    w->last=adc; w->seen=1;
    int bad=adc<64 || adc>4031 || maximum-minimum>128 || abs_i(delta)>512 || minimum<0;
    if (bad) {
        w->locked=1; w->recovery=w->n=w->valid=w->rate_ready=0; w->omega=0;
        return;
    }
    if (w->locked) {
        int trusted=abs_i(adc-HYB_DOWN_ADC)<=600 || abs_i(adc-ZERO_ADC)<=600;
        if (trusted && abs_i(delta)<=96) ++w->recovery;
        else w->recovery=0;
        if (w->recovery<6) { w->valid=w->rate_ready=0; return; }
        w->locked=0; w->n=0;
    }
    for (unsigned i=0;i<4;++i) w->history[i]=w->history[i+1];
    w->history[4]=adc;
    if (w->n<5) ++w->n;
    w->valid=1; w->rate_ready=w->n==5;
    if (w->rate_ready) {
        int slope=-2*w->history[0]-w->history[1]+w->history[3]+2*w->history[4];
        w->omega=slope*(20.0f*3.14159265358979323846f/2080.0f);
    } else w->omega=0;
}

static inline int down_quiet(const AngleWatch *w,int adc) {
    return w->valid && w->rate_ready && abs_i(adc-HYB_DOWN_ADC)<=35 && hfabs(w->omega)<=0.4f;
}
static inline void hybrid_start_check(Capture *c,const AngleWatch *w,uint32_t now,int enc,int adc,int cv,int start,int stop) {
    if (c->phase!=WAITING) return;
    if (now>=900000000U) { end_capture(c,now,enc,EXPIRED_END); return; }
    if (stop || !power_ok(cv) || abs_i(enc)>=HYB_START_COUNTS || !down_quiet(w,adc)) {
        c->ready=c->pressing=c->releasing=0; return;
    }
    if (c->pressing==2) {
        if (start) { c->releasing=0; return; }
        if (!c->releasing) { c->releasing=1; c->released_at=now; }
        if (now-c->released_at>=10000U) begin_capture(c,now,enc);
        return;
    }
    if (!start && c->pressing) c->ready=c->releasing=0;
    if (!start) {
        c->pressing=0;
        if (!c->releasing) { c->releasing=1; c->released_at=now; }
        if (now-c->released_at>=500000U) c->ready=1;
        return;
    }
    c->releasing=0;
    if (!c->ready) return;
    if (!c->pressing) { c->pressing=1; c->pressed_at=now; }
    if (now-c->pressed_at>=300000U) c->pressing=2;
}

// Diagnostic only. The second operator releases USER after seeing both hands clear.
static inline void approach_start(Capture *c,const AngleWatch *w,uint32_t now,int enc,int adc,int cv,int start,int stop) {
    if (c->phase!=WAITING) return;
    if (now>=900000000U) { end_capture(c,now,enc,EXPIRED_END); return; }
    if (stop || !power_ok(cv) || abs_i(enc)>=80 || !w->valid || !w->rate_ready) {
        c->ready=c->pressing=c->releasing=0; return;
    }
    if (c->pressing==2) {
        if (abs_i(adc-ZERO_ADC)>440) { c->ready=c->pressing=c->releasing=0; return; }
        if (start) { c->releasing=0; return; }
        if (!c->releasing) { c->releasing=1; c->released_at=now; }
        if (now-c->released_at>=10000U) begin_capture(c,now,enc);
        return;
    }
    int gap=abs_i(adc-ZERO_ADC);
    if (gap<120 || gap>400 || hfabs(w->omega)>.4f) { c->ready=c->pressing=c->releasing=0; return; }
    if (!start) {
        c->pressing=0;
        if (!c->releasing) { c->releasing=1; c->released_at=now; }
        if (now-c->released_at>=300000U) c->ready=1;
    } else if (c->ready) {
        c->releasing=0;
        if (!c->pressing) { c->pressing=1; c->pressed_at=now; }
        if (now-c->pressed_at>=300000U) c->pressing=2;
    }
}
#ifdef SOLO_ENTRY_DIAGNOSTIC
// Return the projected stopping position toward the wall in the current direction.
// d is travel over 10 ms. The envelope is 1.25*(d/2)^2 plus 10 ms of travel,
// based on measured post-stop encoder changes in trials 35, 40b and 41.
static inline int projected_travel(const Hybrid *h,int enc) {
    int d=hlimit(h->two_step_delta,100);
    int dir=(d>0)-(d<0);
    int at=dir*enc;
    int from_start=dir*(enc-h->run_origin);
    if (from_start>at) at=from_start;
    return at+(5*d*d+15)/16+abs_i(d);
}
static inline void update_launch_brake(Hybrid *h,int enc,int rate_trusted) {
    if (h->rail_brake) {
        if (h->rail_brake*h->two_step_delta>3 || h->rail_brake*h->encoder_delta>1) return;
        if (h->rail_brake*(h->target-enc)>0) h->target=enc;
        h->reference=(float)enc;
        h->centre_filter=h->previous_centre=0;
        h->rail_brake=0;
        return;
    }
    int threshold=rate_trusted ? HYB_BRAKE_PROJECTION : HYB_BLIND_BRAKE_PROJECTION;
    if (abs_i(h->two_step_delta)>=4 && projected_travel(h,enc)>=threshold)
        h->rail_brake=h->two_step_delta>0 ? 1 : -1;
}

// Confirm only a scattered in-range burst, with a fresh quiet burst from the same tick.
static inline int confirmed_angle_burst(const uint16_t first[10],int previous,int second,
                                       int second_min,int second_max) {
    if (second_min<64 || second_max>4031 || second_max-second_min>32 || abs_i(second-previous)>24) return 0;
    int supporting=0;
    for (unsigned i=0;i<10;++i) if (abs_i((int)first[i]-second)<=24) ++supporting;
    return supporting>=8;
}

// Choose an incoming moving state while there is still room to change cart velocity.
// With a stationary pivot the growing component near upright is proportional to theta + tau*omega.
static inline int solo_entry(const AngleWatch *w,int error,int enc,int delta_10ms) {
    const float rad_per_count=3.14159265f/2080.f;
    if (!w->valid || !w->rate_ready || !error || abs_i(error)>SOLO_ENTRY_ADC ||
        error*w->omega>=0 || hfabs(w->omega)<.45f || hfabs(w->omega)>2.8f ||
        abs_i(enc)>700 || abs_i(delta_10ms)>10) return 0;
    if (hfabs(error*rad_per_count+.16315f*w->omega)>.085f) return 0;
    int direction=hsign(w->omega);
    for (int i=1;i<5;++i)
        if ((w->history[i]-w->history[i-1])*direction < -1) return 0;
    float latest_rate=(w->history[4]-w->history[3])*200.f*rad_per_count;
    return hfabs(latest_rate-w->omega)<=.75f;
}
#endif
static inline int hybrid_step(Hybrid *h,const AngleWatch *w,uint32_t elapsed,int enc,int adc) {
    h->request=0;
    if (abs_i(enc)>=HYB_HOME_COUNTS) return TRAVEL_END;
    h->prior_step=h->encoder_delta;
    h->encoder_delta=h->have_encoder ? enc-h->previous_encoder : 0;
    h->two_step_delta=h->encoder_delta+h->prior_step;
    h->previous_encoder=enc; h->have_encoder=1;
#ifdef SOLO_ENTRY_DIAGNOSTIC
    if (h->stage==HYB_SEED || h->stage==HYB_PUMP) {
        if (elapsed>=45000000U) return SWING_TIMEOUT_END;
        update_launch_brake(h,enc,w->valid && w->rate_ready);
    } else if (abs_i(h->two_step_delta)>=4 && projected_travel(h,enc)>=HYB_HOME_COUNTS) {
        return BRAKING_DISTANCE_END;
    }
    if (h->stage==HYB_MOVING_CAPTURE) {
        if (!w->valid || !w->rate_ready) return SENSOR_END;
        if (abs_i(adc-ZERO_ADC)>h->approach_error+4) return APPROACH_ANGLE_END;
        if (elapsed-h->approach_at>=SOLO_APPROACH_US) return APPROACH_TIMEOUT_END;
        if (abs_i(adc-ZERO_ADC)<=SOLO_INNER_ADC) {
            h->stage=HYB_BALANCE; h->balance_at=elapsed; h->balance_origin=enc;
        }
        return NONE;
    }
#endif
    if (h->stage==HYB_FREE_APPROACH) {
        if (!w->valid || !w->rate_ready) return SENSOR_END;
        int error=adc-ZERO_ADC;
        if (elapsed>=1500000U || abs_i(error)>440) return EXPIRED_END;
        if (abs_i(error)>80) return NONE;  // free motion, zero command
        // One pass. An unsuitable handoff ends without ever energizing the drive.
        if (abs_i(error)<40 || error*w->omega>=0.f || hfabs(w->omega)<.35f ||
            hfabs(w->omega)>1.4f || abs_i(enc)>80 || abs_i(h->two_step_delta)>3) return EXPIRED_END;
        h->stage=HYB_BALANCE; h->balance_at=elapsed; h->balance_origin=h->control_origin=enc;
        return NONE;
    }
    if (!w->valid) {
        if (h->stage==HYB_BALANCE) return SENSOR_END;
        if (!h->bad_active) { h->bad_active=1; h->bad_since=elapsed; }
        if (elapsed-h->bad_since>=1500000U) return SENSOR_END;
        return NONE;
    }
    h->bad_active=0;
    if (h->stage==HYB_BALANCE) {
        if (abs_i(adc-ZERO_ADC)>EXIT_ADC) return ANGLE_END;
        if (h->settled && abs_i(enc-h->balance_origin)>=HYB_SETTLED_TRAVEL) return TRAVEL_END;
        if (!h->settled && elapsed-h->balance_at>=500000U) {
            if (abs_i(adc-ZERO_ADC)<=23 && hfabs(w->omega)<=0.3f && abs_i(h->encoder_delta)<=2) ++h->quiet_count;
            else h->quiet_count=0;
            if (h->quiet_count>=60) {
                h->settled=1; h->settled_at=elapsed; h->balance_origin=enc;
            }
        }
        return elapsed-h->balance_at>=90000000U ? TIME_END : NONE;
    }
    if (!h->amplitude) {
        h->amplitude=HYB_INITIAL_AMPLITUDE; h->target=-h->amplitude;
        h->speed=HYB_INITIAL_SPEED; h->reference=(float)enc;
    }
    if (elapsed>=45000000U) return SWING_TIMEOUT_END;
    if (!w->rate_ready) return NONE;
#ifdef SOLO_ENTRY_DIAGNOSTIC
    if (h->rail_brake) return NONE;
#endif
    if (abs_i(adc-ZERO_ADC)<=250 || h->energy>=1.90f) h->near_top=1;
    // Transfer on the first trusted near-upright observation. Settling is
    // decided from quiet motion after transfer, independently of entry.
#ifdef SOLO_ENTRY_DIAGNOSTIC
    if (solo_entry(w,adc-ZERO_ADC,enc,h->two_step_delta)) {
        h->control_origin=h->balance_origin=enc;
        h->approach_at=elapsed; h->approach_error=abs_i(adc-ZERO_ADC);
        if (h->approach_error>SOLO_INNER_ADC) h->stage=HYB_MOVING_CAPTURE;
        else { h->stage=HYB_BALANCE; h->balance_at=elapsed; }
        return NONE;
    }
#else
    if (abs_i(adc-ZERO_ADC)<=80) {
        h->stage=HYB_BALANCE; h->balance_at=elapsed; h->balance_origin=enc; h->control_origin=enc;
        return NONE;
    }
#endif
    if (h->stage==HYB_SEED && (abs_i(enc-h->target)<=40 || elapsed>=800000U)) h->stage=HYB_PUMP;
    if (abs_i(adc-HYB_DOWN_ADC)<=600) {
        float delta=(adc-HYB_DOWN_ADC)*(3.14159265358979323846f/2080.0f);
        float squared=delta*delta;
        float potential=squared*(0.5f-squared*(1.0f/24.0f-squared/720.0f));
        h->energy=0.5f*0.02661697685f*w->omega*w->omega+potential;
    }
    if (abs_i(adc-HYB_DOWN_ADC)>40) h->departed=1;
    int crossing=h->stage==HYB_PUMP && h->departed && abs_i(adc-HYB_DOWN_ADC)<=20 &&
                 hfabs(w->omega)>0.12f && elapsed-h->last_switch>=150000U;
    int retry=h->stage==HYB_PUMP && elapsed-h->last_switch>=2000000U &&
              abs_i(adc-HYB_DOWN_ADC)<=120 && hfabs(w->omega)<=0.5f;
    if (crossing || retry) {
        ++h->switches;
        if (!h->near_top && (h->switches%2==0 || retry)) {
            h->amplitude+=100; if (h->amplitude>HYB_MAX_AMPLITUDE) h->amplitude=HYB_MAX_AMPLITUDE;
            h->speed+=400; if (h->speed>HYB_MAX_SPEED) h->speed=HYB_MAX_SPEED;
        }
        if (h->near_top && h->energy>1.97f && h->switches%2==0 && h->amplitude>250) h->amplitude-=100;
        int direction=crossing ? hsign(w->omega) : (h->target>0 ? -1 : 1);
        int shedding=crossing && h->energy>2.03f;
        if (shedding) direction=-direction;
        // At each lower crossing, shorten the upcoming stroke as measured
        // energy approaches the upright value (2). Keep the previous speed
        // ceiling so the shorter stroke can finish before the rod reaches top.
        int reach=h->amplitude;
        if (h->energy>1.55f) {
            float blend=(h->energy-1.55f)/0.45f;
            if (blend>1.f) blend=1.f;
            reach-= (int)(blend*(reach-220));
        }
        h->target=direction*reach;
#ifdef TRIM_PUMP_STROKE
        // Trial only: shorten one energy-adding stroke across centre near the top energy.
        if (crossing && !shedding && !h->trimmed_stroke && h->energy>=1.86f && enc*direction<=-350) {
            h->target/=2;
            h->trimmed_stroke=1;
        }
#endif
        h->departed=0; h->last_switch=elapsed;
    }
    float step=h->speed*0.005f;
    float remaining=h->target-h->reference;
    h->reference+=remaining>step ? step : (remaining<-step ? -step : remaining);
    float error=enc-h->reference;
    h->centre_filter+=(error-h->centre_filter)*0.2f;
    h->request=hlimit((int)(20.0f*h->centre_filter+300.0f*(h->centre_filter-h->previous_centre)),HYB_SWING_PWM_MAX);
    h->previous_centre=h->centre_filter;
    if (abs_i(h->encoder_delta)>(int)(h->speed*0.005f)+8) h->request=h->encoder_delta>0 ? HYB_SWING_PWM_MAX : -HYB_SWING_PWM_MAX;
    if (abs_i(enc)>=HYB_SOFT_COUNTS && enc*h->request<0) h->request=enc>0 ? HYB_SWING_PWM_MAX : -HYB_SWING_PWM_MAX;
    return NONE;
}
