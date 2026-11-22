#pragma once
// State order: rightward metres, rightward m/s, ADC-positive radians, rad/s.
// Encoder increases leftwards. position_part is SUBTRACTED in existing telemetry.
static inline int state_capture_pwm(int adc,int relative_counts,int delta_10ms,
                                    float omega,int *angle_part,int *position_part) {
    float theta=(adc-ZERO_ADC)*(3.14159265f/2080.f);
    *angle_part=(int)(-7200.f*(26.6361f*theta+4.52645f*omega));
    *position_part=(int)((7200.f/13400.f)*(12.2474f*relative_counts+929.468f*delta_10ms));
    return *angle_part-*position_part;
}
