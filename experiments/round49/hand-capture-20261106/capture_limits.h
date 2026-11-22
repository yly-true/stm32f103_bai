#pragma once
#include <stdint.h>

enum { WAITING, RUNNING, LOCKED };
enum { NONE, TIME_END, TRAVEL_END, STOP_KEY_END, VOLTAGE_END, ANGLE_END,
       HOME_END, EXPIRED_END, TIMING_END, TRACE_END, DIRECTION_PAD_END };
#ifdef RELEASE_TRIAL
#ifndef TRIAL_LENGTH_US
#define TRIAL_LENGTH_US 250000
#endif
#ifdef STREAM_TRIAL
#ifdef HYBRID_TRIAL
#if TRIAL_LENGTH_US != 150000000
#error Invalid autonomous total duration
#endif
#else
#if TRIAL_LENGTH_US != 5000000 && TRIAL_LENGTH_US != 30000000 && TRIAL_LENGTH_US != 60000000
#error Invalid streamed trial duration
#endif
#endif
enum { ENTER_ADC = 17, MAX_TRAVEL = 260, LENGTH_US = TRIAL_LENGTH_US };
#else
#if TRIAL_LENGTH_US != 250000 && TRIAL_LENGTH_US != 500000
#error Only the reviewed 250 ms and 500 ms release profiles are supported
#endif
enum { ENTER_ADC = 17, MAX_TRAVEL = 78, LENGTH_US = TRIAL_LENGTH_US };
#endif
#else
enum { ENTER_ADC = 23, MAX_TRAVEL = 104, LENGTH_US = 400000 };
#endif
#ifndef TRIAL_TRACE_CAP
#define TRIAL_TRACE_CAP 82
#endif
enum { ZERO_ADC = 3131, EXIT_ADC = 92, HOME_RANGE = 260, PERIOD_US = 5000, MAX_INTERVAL_US = 7000,
       PWM_MAX = 1440, PWM_SCALE = 7200, TRACE_CAP = TRIAL_TRACE_CAP };
#ifndef STREAM_TRIAL
typedef char trial_trace_capacity_check[(TRACE_CAP * PERIOD_US >= LENGTH_US) ? 1 : -1];
#endif
#ifndef TRIAL_START_SIDE
#define TRIAL_START_SIDE 0
#endif
#if TRIAL_START_SIDE < -1 || TRIAL_START_SIDE > 1
#error Invalid entry side
#endif
#ifndef TRIAL_POSITION_CAP
#define TRIAL_POSITION_CAP 360
#endif
#if TRIAL_POSITION_CAP != 360 && TRIAL_POSITION_CAP != 1440
#error Invalid position correction limit
#endif
enum { POSITION_PWM_MAX=TRIAL_POSITION_CAP };

typedef struct {
    int phase, ready, releasing, pressing, reason, origin, final_encoder;
    uint32_t released_at, pressed_at, start, finish;
} Capture;

static inline int abs_i(int x) { return x < 0 ? -x : x; }
static inline int cap_pwm(int x) { return x > PWM_MAX ? PWM_MAX : (x < -PWM_MAX ? -PWM_MAX : x); }
static inline unsigned high_ticks(int cmd) { return (unsigned)(abs_i(cmd) * 800 + PWM_SCALE/2) / PWM_SCALE; }
static inline int power_ok(int cv) { return cv >= 950 && cv <= 1320; }
static inline int entry_angle_ok(int adc) {
#if TRIAL_START_SIDE < 0
    return adc >= ZERO_ADC-ENTER_ADC && adc <= ZERO_ADC-6;
#elif TRIAL_START_SIDE > 0
    return adc >= ZERO_ADC+6 && adc <= ZERO_ADC+ENTER_ADC;
#else
    return abs_i(adc-ZERO_ADC) <= ENTER_ADC;
#endif
}

#ifdef RELEASE_TRIAL
enum { LAMP_OFF, USB_IDENTIFY, PRESS_READY, RELEASE_READY, LAMP_FINISHED, USB_UPRIGHT };
static inline int release_lamp_mode(const Capture *c, int cv, int adc) {
    if (c->phase==LOCKED) return LAMP_FINISHED;
    if (c->phase==RUNNING) return LAMP_OFF;
    if (cv>=450 && cv<=600) {
        if (abs_i(adc-1051)<150) return USB_IDENTIFY;
        return entry_angle_ok(adc) ? USB_UPRIGHT : LAMP_OFF;
    }
    if (!power_ok(cv)) return LAMP_OFF;
    if (c->pressing==2) {
#if TRIAL_START_SIDE == 0
        return RELEASE_READY;
#else
        return entry_angle_ok(adc) ? RELEASE_READY : PRESS_READY;
#endif
    }
    return c->ready ? PRESS_READY : LAMP_OFF;
}
static inline int lamp_is_on(int mode, uint32_t now) {
    if (mode==USB_IDENTIFY) return (now/500000U)%2U==0;
    if (mode==PRESS_READY || mode==USB_UPRIGHT) return (now/125000U)%2U==0;
    return mode==RELEASE_READY || mode==LAMP_FINISHED;
}
#endif

static inline void end_capture(Capture *c, uint32_t now, int enc, int reason) {
    c->finish = now;
    c->final_encoder = enc;
    c->reason = reason;
    c->phase = LOCKED;
    c->ready = c->pressing = c->releasing = 0;
}

static inline void begin_capture(Capture *c, uint32_t now, int enc) {
    c->origin = c->final_encoder = enc;
    c->start = now;
    c->phase = RUNNING;
    c->ready = c->pressing = c->releasing = 0;
}

// Only a fresh, debounced local press made after a continuous upright, powered
// release can trigger. Moving into the window while holding the button cannot.
static inline void check_start(Capture *c, uint32_t now, int enc, int adc, int cv, int start, int stop) {
    if (c->phase != WAITING) return;
    if (now >= 900000000U) { end_capture(c, now, enc, EXPIRED_END); return; }
    if (stop || !power_ok(cv) || abs_i(adc-ZERO_ADC) > ENTER_ADC || abs_i(enc) >= HOME_RANGE) {
        c->ready = c->releasing = c->pressing = 0;
        return;
    }
#ifdef RELEASE_TRIAL
    // Holding PA5 requests preparation only. Releasing a qualified hold is
    // the time marker the two operators use to remove contact with the rod.
    if (c->pressing == 2) {
        if (start) { c->releasing=0; return; }
#if TRIAL_START_SIDE != 0
        // Prepare in the full, previously exercised upright window. A side
        // restriction applies to the final physical release, not the hold.
        if (!entry_angle_ok(adc)) {
            c->ready=c->pressing=c->releasing=0;
            return;
        }
#endif
        if (!c->releasing) { c->releasing=1; c->released_at=now; }
        if (now-c->released_at >= 10000U) begin_capture(c,now,enc);
        return;
    }
    if (!start && c->pressing) { c->ready=0; c->releasing=0; }
#endif
    if (!start) {
        c->pressing = 0;
        if (!c->releasing) { c->releasing = 1; c->released_at = now; }
        if (now-c->released_at >= 200000U) c->ready = 1;
        return;
    }
    c->releasing = 0;
    if (!c->ready) return;
    if (!c->pressing) { c->pressing = 1; c->pressed_at = now; }
#ifdef RELEASE_TRIAL
    if (now-c->pressed_at >= 300000U) c->pressing=2;
#else
    if (now-c->pressed_at >= 20000U) begin_capture(c,now,enc);
#endif
}

static inline int stop_reason(const Capture *c, uint32_t now, int enc, int adc, int cv, int stop,
                              uint32_t previous_sample) {
    if (stop) return STOP_KEY_END;
    if (!power_ok(cv)) return VOLTAGE_END;
    if (abs_i(adc-ZERO_ADC) > EXIT_ADC) return ANGLE_END;
    if (abs_i(enc-c->origin) >= MAX_TRAVEL) return TRAVEL_END;
    if (abs_i(enc) >= HOME_RANGE) return HOME_END;
    if (now-c->start >= LENGTH_US) return TIME_END;
    if (now-previous_sample > MAX_INTERVAL_US) return TIMING_END;
    return NONE;
}
