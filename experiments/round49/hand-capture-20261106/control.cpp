#include "App/Control/Config.hpp"
#include "App/PendulumController/Pid.hpp"
#include "capture_limits.h"

static_assert(cfg::kAngleUpright == ZERO_ADC && cfg::kControlPeriodMs*1000 == PERIOD_US &&
              cfg::kPwmPeak == PWM_SCALE, "controller and trial use different units");
static app::Pid angle_controller;
#ifdef POSITION_TRIAL
static app::Pid position_controller;
static float position_filter;
static unsigned position_divider;
static int position_correction;
#endif
#ifdef HALF_ANGLE_KP
constexpr float trial_angle_kp=cfg::kAngleKp*0.5f;
#else
constexpr float trial_angle_kp=cfg::kAngleKp;
#endif

extern "C" void reset_pd() {
    angle_controller.reset();
#ifdef POSITION_TRIAL
    position_controller.reset();
    position_filter=0;
    position_divider=0;
    position_correction=0;
#endif
}
extern "C" int angle_pwm(int raw) {
    const float deg = (raw - cfg::kAngleUpright) / cfg::kAngleCountsPerDegree;
    return static_cast<int>(-angle_controller.update(deg, trial_angle_kp, 0.0f,
                                                     cfg::kAngleKd, cfg::kControlPeriodS));
}

#ifdef POSITION_TRIAL
// Same raw encoder convention and Balance - Position combination as the
// upstream controller. The component and final actuator limits are separate.
static int position_tick(int relative_encoder) {
    if (++position_divider>=5) {
        position_divider=0;
        position_filter+=(relative_encoder-position_filter)*0.2f;
        int request=static_cast<int>(position_controller.update(position_filter,5.0f,0.0f,
                                                                 100.0f*0.025f,0.025f));
        position_correction=request>POSITION_PWM_MAX ? POSITION_PWM_MAX : (request<-POSITION_PWM_MAX ? -POSITION_PWM_MAX : request);
    }
    return position_correction;
}
extern "C" int guarded_position_pwm(int raw, int relative_encoder, int *angle_part, int *position_part) {
    *angle_part=angle_pwm(raw);
    *position_part=position_tick(relative_encoder);
    return *angle_part-*position_part;
}
#ifdef HYBRID_TRIAL
extern "C" int guarded_hybrid_pwm(int raw,int relative_encoder,float omega,int kd_per_delta,int *angle_part,int *position_part) {
    const float derivative_counts=omega*(2080.0f/3.14159265358979323846f)*cfg::kControlPeriodS;
    *angle_part=static_cast<int>(-200.0f*(raw-ZERO_ADC)-kd_per_delta*derivative_counts);
    *position_part=position_tick(relative_encoder);
    return *angle_part-*position_part;
}
#endif
#endif
