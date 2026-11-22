#include "capture_limits.h"
#include <cassert>
#include <cstdio>
#ifdef STREAM_TRIAL
#include "telemetry_stream.h"
#endif

extern "C" void reset_pd();
extern "C" int angle_pwm(int);
#ifdef POSITION_TRIAL
extern "C" int guarded_position_pwm(int,int,int*,int*);
#endif
static constexpr int entry_adc=ZERO_ADC+TRIAL_START_SIDE*10;

static void enter(Capture &c) {
    check_start(&c,0,1,entry_adc,1240,0,0);
    check_start(&c,200000,1,entry_adc,1240,0,0);
    assert(c.ready);
    check_start(&c,205000,1,entry_adc,1240,1,0);
    check_start(&c,224999,1,entry_adc,1240,1,0);
    assert(c.phase==WAITING);
#ifdef RELEASE_TRIAL
    check_start(&c,505000,1,entry_adc,1240,1,0);
    assert(c.phase==WAITING && c.pressing==2);
    check_start(&c,510000,1,entry_adc,1240,0,0);
    check_start(&c,519999,1,entry_adc,1240,0,0);
    assert(c.phase==WAITING);
    check_start(&c,520000,1,entry_adc,1240,0,0);
#else
    check_start(&c,225000,1,entry_adc,1240,1,0);
#endif
    assert(c.phase==RUNNING && c.origin==1);
}
int main() {
#ifdef STREAM_TRIAL
    const int vector[]={0x12345678,-1,3131};
    assert(stream_frame(2,vector,3));
    uint8_t bytes[18];
    for (unsigned i=0;i<sizeof(bytes);++i) assert(stream_pop(&bytes[i]));
    assert(bytes[0]==0xa5 && bytes[1]==0x5a && bytes[2]==2 && bytes[3]==12);
    assert(bytes[4]==0x78 && bytes[5]==0x56 && bytes[6]==0x34 && bytes[7]==0x12);
    assert(bytes[16]==0xd5 && bytes[17]==0xc3);
    assert(!stream_pop(bytes));
    unsigned frames=0;
    while (stream_frame(2,vector,3)) ++frames;
    unsigned received=0;
    while (stream_pop(bytes)) ++received;
    assert(frames>10 && received==frames*18);
#endif
    Capture c={};
    check_start(&c,0,0,ZERO_ADC,533,0,0);
    check_start(&c,250000,0,ZERO_ADC,533,0,0);
    assert(!c.ready);
    check_start(&c,255000,0,ZERO_ADC,1240,1,0);
    check_start(&c,600000,0,ZERO_ADC,1240,1,0);
    assert(c.phase==WAITING && !c.ready);
    c=Capture{}; enter(c);
    auto why=[&](int e,int a,int v,int stop,unsigned dt) {
        return stop_reason(&c,c.start+dt,e,a,v,stop,c.start);
    };
    assert(why((int)MAX_TRAVEL<(int)HOME_RANGE ? MAX_TRAVEL : HOME_RANGE-1,ZERO_ADC+92,950,0,7000)==NONE);
    assert(why(1+MAX_TRAVEL,ZERO_ADC,1240,0,100)==TRAVEL_END);
    assert(why(1-MAX_TRAVEL,ZERO_ADC,1240,0,100)==TRAVEL_END);
    assert(why(1,ZERO_ADC+93,1240,0,100)==ANGLE_END);
    assert(why(1,ZERO_ADC-93,1240,0,100)==ANGLE_END);
    assert(why(1,ZERO_ADC,949,0,100)==VOLTAGE_END);
    assert(why(1,ZERO_ADC,1321,0,100)==VOLTAGE_END);
    assert(why(1,ZERO_ADC,1240,1,100)==STOP_KEY_END);
    assert(why(1,ZERO_ADC,1240,0,7001)==TIMING_END);
    assert(stop_reason(&c,c.start+LENGTH_US-1,1,ZERO_ADC,1240,0,c.start+LENGTH_US-5000)==NONE);
    assert(stop_reason(&c,c.start+LENGTH_US,1,ZERO_ADC,1240,0,c.start+LENGTH_US-5000)==TIME_END);
    Capture home=c; home.origin=220;
    assert(stop_reason(&home,home.start+10,260,ZERO_ADC,1240,0,home.start)==HOME_END);
    end_capture(&c,c.start+30000,10,TRAVEL_END);
    check_start(&c,2600000,0,ZERO_ADC,1200,0,0);
    check_start(&c,2800000,0,ZERO_ADC,1200,1,0);
    assert(c.phase==LOCKED && !c.ready);
    for (int i=0;i<5;++i) {
        Capture blocked={};
        check_start(&blocked,0,0,ZERO_ADC,1240,0,0);
        check_start(&blocked,190000,0,ZERO_ADC,1240,0,0);
        check_start(&blocked,210000,i==0?260:0,i==1?ZERO_ADC+24:ZERO_ADC,
                    i==2?949:1240,i==3,i==4);
        assert(!blocked.ready && blocked.phase==WAITING);
    }
    Capture expired={};
    check_start(&expired,900000000,0,ZERO_ADC,1240,0,0);
    assert(expired.phase==LOCKED && expired.reason==EXPIRED_END);
#ifdef RELEASE_TRIAL
    Capture lamp_test={};
    assert(release_lamp_mode(&lamp_test,528,1050)==USB_IDENTIFY);
    assert(lamp_is_on(USB_IDENTIFY,490000) && !lamp_is_on(USB_IDENTIFY,510000));
    assert(release_lamp_mode(&lamp_test,528,entry_adc)==USB_UPRIGHT);
    assert(release_lamp_mode(&lamp_test,528,ZERO_ADC+18)==LAMP_OFF);
    assert(!lamp_is_on(USB_UPRIGHT,130000) && lamp_is_on(USB_UPRIGHT,260000));
    assert(release_lamp_mode(&lamp_test,1240,entry_adc)==LAMP_OFF);
    lamp_test.ready=1; assert(release_lamp_mode(&lamp_test,1240,entry_adc)==PRESS_READY);
    lamp_test.pressing=2; assert(release_lamp_mode(&lamp_test,1240,entry_adc)==RELEASE_READY);
    lamp_test.phase=RUNNING; assert(release_lamp_mode(&lamp_test,1240,ZERO_ADC)==LAMP_OFF);
    lamp_test.phase=LOCKED; assert(release_lamp_mode(&lamp_test,1240,ZERO_ADC)==LAMP_FINISHED);
    for (int scenario=0;scenario<4;++scenario) {
        Capture t={};
        check_start(&t,0,0,entry_adc,1240,0,0);
        check_start(&t,200000,0,entry_adc,1240,0,0);
        check_start(&t,210000,0,entry_adc,1240,1,0);
        if (scenario==0) {
            check_start(&t,400000,0,entry_adc,1240,0,0); // short hold cannot start
        } else {
            check_start(&t,515000,0,entry_adc,1240,1,0);
            assert(t.pressing==2);
            check_start(&t,520000,0,scenario==1?ZERO_ADC+18:entry_adc,
                        scenario==2?528:1240,1,scenario==3);
        }
        check_start(&t,530000,0,entry_adc,1240,0,0);
        check_start(&t,545000,0,entry_adc,1240,0,0);
        assert(t.phase==WAITING && !t.ready);
    }
#endif
#if TRIAL_START_SIDE != 0
    const int wrong_adcs[] = {ZERO_ADC, ZERO_ADC-TRIAL_START_SIDE*10, ZERO_ADC+TRIAL_START_SIDE*5};
    for (int wrong : wrong_adcs) {
        Capture blocked={};
        for (unsigned now=0;now<1000000;now+=5000)
            check_start(&blocked,now,0,wrong,1240,now>=250000 && now<750000,0);
        assert(blocked.phase==WAITING);
    }
    Capture lost={};
    check_start(&lost,0,0,entry_adc,1240,0,0);
    check_start(&lost,200000,0,entry_adc,1240,0,0);
    check_start(&lost,205000,0,entry_adc,1240,1,0);
    check_start(&lost,505000,0,entry_adc,1240,1,0);
    assert(lost.pressing==2);
    check_start(&lost,510000,0,ZERO_ADC,1240,0,0);
    assert(lost.phase==WAITING && !lost.ready && !lost.pressing);
    Capture adjust={};
    check_start(&adjust,0,0,ZERO_ADC,1240,0,0);
    check_start(&adjust,200000,0,ZERO_ADC,1240,0,0);
    assert(adjust.ready);
    check_start(&adjust,205000,0,ZERO_ADC,1240,1,0);
    check_start(&adjust,505000,0,ZERO_ADC,1240,1,0);
    assert(adjust.pressing==2 && release_lamp_mode(&adjust,1240,ZERO_ADC)==PRESS_READY);
    check_start(&adjust,510000,0,entry_adc,1240,1,0);
    assert(adjust.pressing==2 && release_lamp_mode(&adjust,1240,entry_adc)==RELEASE_READY);
    check_start(&adjust,515000,0,entry_adc,1240,0,0);
    check_start(&adjust,525000,0,entry_adc,1240,0,0);
    assert(adjust.phase==RUNNING);
#endif
    reset_pd(); assert(angle_pwm(ZERO_ADC)==0);
#ifdef POSITION_TRIAL
    int angle_part=0,position_part=0;
    reset_pd();
    for (int i=0;i<100;++i)
        assert(guarded_position_pwm(ZERO_ADC,0,&angle_part,&position_part)==0 && !angle_part && !position_part);
    const int position_signs[]={-1,1};
    for (int sign : position_signs) {
        reset_pd();
        for (int i=0;i<40;++i) {
            int request=guarded_position_pwm(ZERO_ADC,sign*1000,&angle_part,&position_part);
            assert(angle_part==0 && abs_i(position_part)<=POSITION_PWM_MAX && request==-position_part);
        }
        assert(position_part==sign*POSITION_PWM_MAX);
        reset_pd();
        assert(guarded_position_pwm(ZERO_ADC,0,&angle_part,&position_part)==0 && !position_part);
    }
    reset_pd(); assert(angle_pwm(ZERO_ADC)==0);
#endif
    auto close=[](int a,int b) { return abs_i(a-b)<=1; };
#ifdef HALF_ANGLE_KP
    const int kp_per_count=200;
#else
    const int kp_per_count=400;
#endif
    assert(close(angle_pwm(ZERO_ADC-1),kp_per_count+100));
    assert(close(angle_pwm(ZERO_ADC-1),kp_per_count));
    assert(close(angle_pwm(ZERO_ADC+1),-kp_per_count-200));
    reset_pd(); assert(close(angle_pwm(ZERO_ADC-15),15*kp_per_count));
    for (int req=-60000;req<=60000;++req) {
        int cmd=cap_pwm(req);
        assert(abs_i(cmd)<=1440 && high_ticks(cmd)<=160);
    }
    assert(high_ticks(1440)==160 && high_ticks(-1440)==160 && high_ticks(0)==0);
    std::puts("PASS local-key interlock, timing, signed travel, angle, supply, stop, lockout, upstream PD and 120001 PWM inputs");
}
