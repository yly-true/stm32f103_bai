#include "swing_logic.h"
#include "state_capture.h"
#include <cassert>
#include <cstdio>
int main() {
    int a,p;
    assert(state_capture_pwm(ZERO_ADC,0,0,0,&a,&p)==0);
    assert(state_capture_pwm(ZERO_ADC,0,0,1,&a,&p)==-32590 && p==0);
    assert(state_capture_pwm(ZERO_ADC,0,2,0,&a,&p)<-990 && a==0);
    int left=state_capture_pwm(ZERO_ADC-60,40,2,.8f,&a,&p);
    assert(left==a-p && left==-state_capture_pwm(ZERO_ADC+60,-40,-2,-.8f,&a,&p));
    std::puts("PASS four-state capture units, derivative sign, symmetry and telemetry");
}
