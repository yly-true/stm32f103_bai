#include "swing_logic.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
#include <cstring>

int main(int argc,char **argv) {
#ifdef MERGED_HOLD
    if (argc==2 && std::strcmp(argv[1],"--pair-replay")==0) {
        Hybrid h={};
        int near,command;
        while (std::scanf("%d%d",&near,&command)==2)
            std::printf("%d\n",paired_motor_output(&h,near,command));
        return 0;
    }
    for (int side : {-1,1}) {
        Hybrid h={};
        assert(paired_motor_output(&h,1,side*2000)==side*2000);
        assert(paired_motor_output(&h,1,-side*3000)==side*2000);
        assert(paired_motor_output(&h,1,side*3000)==0);
        assert(paired_motor_output(&h,1,-side*3000)==0);
        assert(paired_motor_output(&h,1,-side*1000)==-side*2000);
        assert(paired_motor_output(&h,1,side*200)==-side*2000);
        assert(paired_motor_output(&h,0,side*6000)==side*6000);
        assert(paired_motor_output(&h,1,-side*3000)==-side*3000);
#ifdef CARRY_HOLD
        paired_motor_output(&h,1,side*350);
        assert(paired_motor_output(&h,1,side*350)==0 && h.pair_remainder==side*350);
        paired_motor_output(&h,1,side*350);
        assert(paired_motor_output(&h,1,side*350)==side*700 && !h.pair_remainder);
        paired_motor_output(&h,1,side*350);
        assert(paired_motor_output(&h,1,side*350)==0);
        paired_motor_output(&h,1,side*3000);
        assert(paired_motor_output(&h,1,side*3000)==side*3000 && h.pair_remainder==side*350);
        assert(paired_motor_output(&h,0,-side*6900)==-side*6900 && !h.pair_remainder);
#endif
    }
#endif
#ifdef LIMITED_HOLD
    for (int side : {-1,1}) {
        assert(hold_pwm_limit(1999999,0,0,0)==HYB_SWING_PWM_MAX);
        int limit=hold_pwm_limit(2000000,side*32,side*1.25f,side*24);
        assert(limit==3000 && hlimit(side*6000,limit)==side*3000 && hlimit(-side*6000,limit)==-side*3000);
        assert(hlimit(side*100,limit)==side*100);
        assert(hold_pwm_limit(2000000,side*33,0,0)==HYB_SWING_PWM_MAX);
        assert(hold_pwm_limit(2000000,0,side*1.26f,0)==HYB_SWING_PWM_MAX);
        assert(hold_pwm_limit(2000000,0,0,side*25)==HYB_SWING_PWM_MAX);
    }
#endif
#ifdef GENTLE_HOLD
    if (argc==2 && std::strcmp(argv[1],"--quiet-replay")==0) {
        Hybrid state={};
        unsigned age;
        int error,slope,delta,request;
        while (std::scanf("%u%d%d%d%d",&age,&error,&slope,&delta,&request)==5) {
            state.two_step_delta=delta;
            int out=quiet_motor_output(&state,age,error,slope*(20.f*3.14159265f/2080.f),hlimit(request,HYB_SWING_PWM_MAX));
            std::printf("%d %d\n",state.hold_coasting,out);
        }
        return 0;
    }
    for (int side : {-1,1}) {
        Hybrid quiet={};
        assert(quiet_motor_output(&quiet,0,0,0,side*100)==side*100);
        assert(quiet_motor_output(&quiet,2000000,0,0,-side*100)==0);
        assert(quiet_motor_output(&quiet,2005000,0,0,side*100)==side*100);
        assert(quiet_motor_output(&quiet,2010000,0,0,-side*100)==0);
        assert(quiet_motor_output(&quiet,2015000,0,0,-side*100)==0);
        assert(quiet_motor_output(&quiet,2020000,0,0,-side*100)==-side*100);
        assert(quiet_motor_output(&quiet,2025000,side*33,0,side*100)==side*100);
        quiet.two_step_delta=25;
        assert(quiet_motor_output(&quiet,2030000,0,0,-side*100)==-side*100);
        quiet.two_step_delta=0;
        assert(quiet_motor_output(&quiet,2035000,0,1.26f,side*100)==side*100);
    }
#endif
#ifdef SOLO_ENTRY_DIAGNOSTIC
    if (argc==2 && std::strcmp(argv[1],"--entry-cases")==0) {
        int id,enc,delta,raw[5];
        while (std::scanf("%d%d%d%d%d%d%d%d",&id,&enc,&delta,&raw[0],&raw[1],&raw[2],&raw[3],&raw[4])==8) {
            AngleWatch observed={};
            for (int adc:raw) angle_watch(&observed,adc,adc,adc);
            if (solo_entry(&observed,raw[4]-ZERO_ADC,enc,delta))
                std::printf("%d %d\n",id,(int)(observed.omega*1000));
        }
        return 0;
    }
#endif
    if (argc==2 && std::strcmp(argv[1],"--watch")==0) {
        AngleWatch w={}; int adc,minimum,maximum;
        while (std::scanf("%d%d%d",&adc,&minimum,&maximum)==3) {
            angle_watch(&w,adc,minimum,maximum);
            std::printf("%d %d %d %d\n",w.valid,w.rate_ready,w.locked,(int)(w.omega*1000));
        }
        return 0;
    }
    AngleWatch w={};
    for (int i=0;i<20;++i) angle_watch(&w,1050,1048,1052);
    assert(down_quiet(&w,1050));
    Capture c={};
    hybrid_start_check(&c,&w,0,0,1050,528,0,0);
    hybrid_start_check(&c,&w,1000000,0,1050,528,1,0);
    assert(c.phase==WAITING && !c.ready);
    c=Capture{};
    hybrid_start_check(&c,&w,0,0,1050,1240,0,0);
    hybrid_start_check(&c,&w,500000,0,1050,1240,0,0);
    assert(c.ready);
    hybrid_start_check(&c,&w,505000,0,1050,1240,1,0);
    hybrid_start_check(&c,&w,805000,0,1050,1240,1,0);
    assert(c.pressing==2 && c.phase==WAITING);
    hybrid_start_check(&c,&w,810000,0,1050,1240,0,0);
    hybrid_start_check(&c,&w,820000,0,1050,1240,0,0);
    assert(c.phase==RUNNING);
#ifdef SOLO_ENTRY_DIAGNOSTIC
    Capture near_centre={};
    hybrid_start_check(&near_centre,&w,0,138,1050,1240,0,0);
    hybrid_start_check(&near_centre,&w,510000,138,1050,1240,0,0);
    assert(near_centre.ready);
#endif
    Hybrid h={}; h.stage=HYB_SEED;
    assert(hybrid_step(&h,&w,0,0,1050)==NONE && h.request>0 && h.target==-400);
    assert(hybrid_step(&h,&w,900000,0,1050)==NONE && h.stage==HYB_PUMP);
    angle_watch(&w,4095,4094,4095);
    assert(!w.valid && w.locked);
    angle_watch(&w,340,330,350);
    assert(!w.valid);
    angle_watch(&w,3550,3530,3570);
    assert(!w.valid);
    assert(hybrid_step(&h,&w,1000000,0,3550)==NONE && !h.request);
    assert(hybrid_step(&h,&w,2500000,0,3550)==SENSOR_END);
    w=AngleWatch{};
    for (int i=0;i<20;++i) angle_watch(&w,1050,1048,1052);
    h=Hybrid{};h.stage=HYB_PUMP;h.departed=1;w.omega=-1;
    assert(hybrid_step(&h,&w,300000,0,1050)==NONE && h.target<0);
    h.departed=1;
    w.omega=1;
    assert(hybrid_step(&h,&w,600000,0,1050)==NONE && h.target>0 && h.amplitude==500);
    assert(hybrid_step(&h,&w,610000,HYB_HOME_COUNTS,1050)==TRAVEL_END);
    h.near_top=1;
    const int frozen_amplitude=h.amplitude;
    h.departed=1;w.omega=-1;
    assert(hybrid_step(&h,&w,900000,0,1050)==NONE && h.amplitude==frozen_amplitude);
    Hybrid trim={}; trim.stage=HYB_PUMP; trim.amplitude=800; trim.speed=4800;
    trim.near_top=1; trim.departed=1; w.omega=12.5f;
    assert(hybrid_step(&trim,&w,300000,0,1050)==NONE);
    assert(trim.energy>2.03f && trim.target==-220 && trim.speed==4800);
#ifdef TRIM_PUMP_STROKE
    for (int side : {-1,1}) {
        Hybrid single={}; single.stage=HYB_PUMP; single.amplitude=900; single.speed=4400;
        single.departed=single.have_encoder=1; single.previous_encoder=side*703;
        AngleWatch lower=w; lower.omega=-side*12.02f;
        assert(hybrid_step(&single,&lower,400000,side*703,1050)==NONE && single.trimmed_stroke);
        const int trimmed_target=single.target;
        assert(trimmed_target*side<0 && abs_i(trimmed_target)<200);
        single.departed=1;
        assert(hybrid_step(&single,&lower,800000,side*703,1050)==NONE);
        assert(abs_i(single.target-2*trimmed_target)<=1);
        single.trimmed_stroke=0; single.departed=1; lower.omega=-side*10.f;
        assert(hybrid_step(&single,&lower,1200000,side*703,1050)==NONE && !single.trimmed_stroke);
    }
#endif
    for (int value=-20000;value<=20000;++value) {
        int limited=hlimit(value,HYB_SWING_PWM_MAX);
        assert(abs_i(limited)<=6900 && high_ticks(limited)<800);
    }
#ifdef SOLO_ENTRY_DIAGNOSTIC
    // Synthetic examples with the last accepted mean from 40b; its individual
    // terminal conversions were not recorded. The fresh burst must agree independently.
    uint16_t scattered[10]={3140,3141,3139,3139,3141,3142,3139,3140,3141,3526};
    assert(confirmed_angle_burst(scattered,3143,3139,3137,3142));
    assert(!confirmed_angle_burst(scattered,3143,3139,3110,3188));
    assert(!confirmed_angle_burst(scattered,3143,3174,3172,3176));
    assert(!confirmed_angle_burst(scattered,3143,-1,-1,-1));
    scattered[0]=scattered[1]=3200;
    assert(!confirmed_angle_burst(scattered,3143,3139,3138,3141));
    // Measured incoming sample from run 38, then mirrored to verify both directions.
    for (int side : {-1,1}) {
        // Run 41 had no further PWM after +723 and reached +1300 by inertia.
        Hybrid blind={}; blind.stage=HYB_PUMP; blind.have_encoder=1;
        blind.target=side*900; blind.previous_encoder=side*701; blind.encoder_delta=side*23;
        AngleWatch missing={};
        assert(hybrid_step(&blind,&missing,9440100,side*723,466)==NONE && blind.rail_brake==side && !blind.request);
        assert(projected_travel(&blind,side*723)==1401);
        assert(hybrid_step(&blind,&missing,9445100,side*745,468)==NONE && blind.rail_brake==side);
        assert(hybrid_step(&blind,&missing,9450100,side*746,465)==NONE && blind.rail_brake==side);
        assert(hybrid_step(&blind,&missing,9455100,side*746,461)==NONE && !blind.rail_brake && blind.target==side*746);
        Hybrid fast={}; fast.stage=HYB_BALANCE; fast.have_encoder=1;
        fast.previous_encoder=side*1475; fast.encoder_delta=side*25;
        assert(hybrid_step(&fast,&w,2000000,side*1500,ZERO_ADC)==BRAKING_DISTANCE_END);
        Hybrid slow={}; slow.stage=HYB_BALANCE; slow.have_encoder=1; slow.previous_encoder=side*1499;
        assert(hybrid_step(&slow,&w,2000000,side*1500,ZERO_ADC)==NONE);
        AngleWatch incoming={};
        for (int q : {255,254,243,233,224}) {
            int raw=ZERO_ADC+side*q;
            angle_watch(&incoming,raw,raw-1,raw+1);
        }
        int entry=ZERO_ADC+side*224;
        assert(solo_entry(&incoming,entry-ZERO_ADC,side*(-208),side*8));
        assert(!solo_entry(&incoming,entry-ZERO_ADC,701,0));
        assert(!solo_entry(&incoming,entry-ZERO_ADC,0,11));
        assert(!solo_entry(&incoming,ZERO_ADC-entry,0,0));
        Hybrid early={}; early.stage=HYB_PUMP;
        assert(hybrid_step(&early,&incoming,1000000,side*(-208),entry)==NONE);
        assert(early.stage==HYB_MOVING_CAPTURE && early.control_origin==side*(-208));
        Hybrid copy=early; incoming.valid=0;
        assert(hybrid_step(&copy,&incoming,1005000,side*(-208),entry)==SENSOR_END);
        incoming.valid=1;
        assert(hybrid_step(&copy,&incoming,1005000,side*(-208),entry+side*5)==APPROACH_ANGLE_END);
        assert(hybrid_step(&copy,&incoming,1300000,side*(-208),entry)==APPROACH_TIMEOUT_END);
        assert(hybrid_step(&early,&incoming,1100000,side*(-191),ZERO_ADC+side*80)==NONE);
        assert(early.stage==HYB_BALANCE && early.balance_at==1100000 && early.control_origin==side*(-208));
    }
    AngleWatch spike={};
    for (int raw : {3193,3188,3183,3178,3185}) angle_watch(&spike,raw,raw,raw);
    assert(!solo_entry(&spike,54,-217,0));
    AngleWatch direct={};
    for (int raw : {3052,3055,3058,3061,3064}) angle_watch(&direct,raw,raw,raw);
    h=Hybrid{}; h.stage=HYB_PUMP;
    direct.rate_ready=0;
    assert(hybrid_step(&h,&direct,995000,0,3064)==NONE && h.stage==HYB_PUMP);
    direct.rate_ready=1;
    assert(hybrid_step(&h,&direct,1000000,0,3064)==NONE && h.stage==HYB_BALANCE);
    std::puts("PASS solo moving entry, mirrored measured rate, outlier rejection, cart guards, bounded approach, continuous reference and direct entry");
#else
    // A fast pass or a moving cart previously kept the pump active here.
    // Exercise both boundaries, plus the recorded upright pass from run 37.
    for (int side : {-1,1}) {
        Hybrid check={}; check.stage=HYB_PUMP;
        w.omega=side*4.f; w.rate_ready=1;
        assert(hybrid_step(&check,&w,950000,side*700,ZERO_ADC+side*81)==NONE);
        assert(check.stage==HYB_PUMP);
        assert(hybrid_step(&check,&w,955000,side*730,ZERO_ADC+side*80)==NONE);
        assert(check.stage==HYB_BALANCE && check.balance_at==955000 && !check.settled);
    }
    w.omega=-.816f; w.rate_ready=0;
    assert(hybrid_step(&h,&w,995000,-788,ZERO_ADC)==NONE && h.stage==HYB_PUMP);
    w.rate_ready=1;
    assert(hybrid_step(&h,&w,1000000,-788,ZERO_ADC)==NONE && h.stage==HYB_BALANCE);
#endif
    assert(hybrid_step(&h,&w,1020000,0,3131+93)==ANGLE_END);
    h.previous_encoder=HYB_SETTLED_TRAVEL; h.encoder_delta=0;
    assert(hybrid_step(&h,&w,1020000,HYB_SETTLED_TRAVEL,3131)==NONE);
    h.settled=1;h.balance_origin=0;
    assert(hybrid_step(&h,&w,1025000,HYB_SETTLED_TRAVEL,3131)==TRAVEL_END);
    h.previous_encoder=0; h.encoder_delta=0;
#ifdef SOLO_ENTRY_DIAGNOSTIC
    assert(hybrid_step(&h,&w,h.balance_at+5000000U,0,3131)==NONE);
    assert(hybrid_step(&h,&w,h.balance_at+60000000U,0,3131)==NONE);
#endif
    assert(hybrid_step(&h,&w,h.balance_at+90000000U,0,3131)==TIME_END);
    Hybrid settling={}; settling.stage=HYB_BALANCE; w.omega=0;
    for (unsigned i=0;i<61;++i) assert(hybrid_step(&settling,&w,500000+i*5000,10,3131)==NONE);
    assert(settling.settled && settling.balance_origin==10 && settling.settled_at>=500000U);
    settling.previous_encoder=10+HYB_SETTLED_TRAVEL;
    assert(hybrid_step(&settling,&w,900000,10+HYB_SETTLED_TRAVEL,3131)==TRAVEL_END);
    Capture hand={}; AngleWatch obs={}; obs.valid=obs.rate_ready=1;
    approach_start(&hand,&obs,0,0,ZERO_ADC-190,1240,0,0);
    approach_start(&hand,&obs,310000,0,ZERO_ADC-190,1240,0,0);
    approach_start(&hand,&obs,320000,0,ZERO_ADC-190,1240,1,0);
    approach_start(&hand,&obs,630000,0,ZERO_ADC-190,1240,1,0);
    obs.omega=.8f;
    approach_start(&hand,&obs,640000,0,ZERO_ADC-100,1240,1,0);
    assert(hand.phase==WAITING && hand.pressing==2);
    approach_start(&hand,&obs,650000,0,ZERO_ADC-90,1240,0,0);
    approach_start(&hand,&obs,660000,0,ZERO_ADC-84,1240,0,0);
    assert(hand.phase==RUNNING);
    Hybrid approach={}; approach.stage=HYB_FREE_APPROACH;
    assert(hybrid_step(&approach,&obs,0,0,ZERO_ADC-84)==NONE && !approach.request);
    assert(approach.stage==HYB_FREE_APPROACH);
    assert(hybrid_step(&approach,&obs,5000,0,ZERO_ADC-79)==NONE && approach.stage==HYB_BALANCE);
    for (float speed : {0.f,-.8f,2.f}) {
        approach=Hybrid{}; approach.stage=HYB_FREE_APPROACH; obs.omega=speed;
        assert(hybrid_step(&approach,&obs,5000,0,ZERO_ADC-79)==EXPIRED_END && !approach.request);
    }
    std::puts("PASS free-approach coast, independent local release, direction/rate qualification");
    std::puts("PASS down-only local release, supply interlock, seed, energy direction, seam lockout, zero-output invalid data, capture, travel, angle and balance timeout");
}
