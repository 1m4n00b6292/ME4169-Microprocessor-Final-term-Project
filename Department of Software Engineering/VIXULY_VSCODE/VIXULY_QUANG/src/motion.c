#include <Arduino.h>
#include "motor.h"
#include "motion.h"
#include "config.h"

/* gains (Q8) */
static int16_t  s_kp = 0, s_ki = 0, s_kd = 0, s_kff = 0;
static uint8_t  s_period = MOTION_PERIOD_MS;

/* derived per-period constants */
static int32_t  s_ki_dt  = 0;   /* Q16 : ki * dt        */
static int32_t  s_kd_dt  = 0;   /* Q8  : kd / dt (1/s)  */
static int16_t  s_step   = 0;   /* ramp step per period */
static uint16_t s_accel  = 0;

/* state */
static int16_t  s_target = 0, s_setpoint = 0, s_out = 0;
static int16_t  s_prev_meas = 0;
static int32_t  s_integ  = 0;   /* Q16 duty units */
static int16_t  s_dfilt  = 0;   /* Q8 duty units  */
static uint8_t  s_run    = 0;
static uint32_t s_last   = 0;

static void recompute(void)
{
    s_ki_dt = (int32_t)s_ki * s_period * 256L / 1000L;   /* Q8 * ms -> Q16 per sample */
    s_kd_dt = (int32_t)s_kd * 1000L / s_period;          /* Q8 / s per sample         */
    s_step  = (int16_t)((uint32_t)s_accel * s_period / 1000UL);
    if (s_accel && s_step == 0) s_step = 1;
}

static void reset_state(int16_t meas)
{
    s_integ = 0;
    s_dfilt = 0;
    s_prev_meas = meas;
    s_setpoint = 0;
    s_out = 0;
}

void motion_init(void)
{
    motor_init();
    reset_state(0);
    s_run = 0;
    recompute();
}

void motion_set_gains(int16_t kp, int16_t ki, int16_t kd)
{
    s_kp = kp; s_ki = ki; s_kd = kd;
    recompute();
}

void motion_set_feedforward(int16_t kff) { s_kff = kff; }

void motion_set_period(uint8_t ms)
{
    if (ms == 0) ms = 1;
    s_period = ms;
    recompute();
}

void motion_set_accel(uint16_t units_per_s)
{
    s_accel = units_per_s;
    recompute();
}

void    motion_set_target(int16_t v) { s_target = v; }
int16_t motion_get_target(void)      { return s_target; }
int16_t motion_get_setpoint(void)    { return s_setpoint; }
int16_t motion_get_output(void)      { return s_out; }
uint8_t motion_is_running(void)      { return s_run; }

void motion_start(void)
{
    if (s_run) return;
    reset_state(s_prev_meas);
    s_last = millis();
    s_run = 1;
}

void motion_stop(void)
{
    s_run = 0;
    reset_state(s_prev_meas);
    motor_brake();
}

uint8_t motion_update(int16_t measured)
{
    if (!s_run) return 0;

    uint32_t now = millis();
    if ((uint32_t)(now - s_last) < s_period) return 0;
    s_last += s_period;
    if ((uint32_t)(now - s_last) >= s_period) s_last = now;  /* resync after long stall */

    /* --- setpoint ramp --- */
    if (s_accel == 0) {
        s_setpoint = s_target;
    } else if (s_setpoint < s_target) {
        int32_t v = (int32_t)s_setpoint + s_step;
        s_setpoint = (v > s_target) ? s_target : (int16_t)v;
    } else if (s_setpoint > s_target) {
        int32_t v = (int32_t)s_setpoint - s_step;
        s_setpoint = (v < s_target) ? s_target : (int16_t)v;
    }

    /* --- error --- */
    int32_t e = (int32_t)s_setpoint - measured;
    if (e >  32000L) e =  32000L;
    if (e < -32000L) e = -32000L;

    /* P */
    int32_t p = ((int32_t)s_kp * e) >> 8;

    /* D on measurement (no kick on setpoint change), low-pass filtered */
    int32_t draw = -((s_kd_dt * (int32_t)(measured - s_prev_meas)) >> 8);   /* Q8 */
    s_prev_meas = measured;
    s_dfilt += (int16_t)((draw - s_dfilt) >> MOTION_D_FILTER_SHIFT);
    int32_t d = s_dfilt >> 8;

    /* feed-forward */
    int32_t ff = ((int32_t)s_kff * s_setpoint) >> 8;

    /* I with clamp + conditional integration (anti-windup) */
    const int32_t imax = (int32_t)MOTION_OUT_MAX << 16;
    int32_t i_new = s_integ + s_ki_dt * e;
    if (i_new >  imax) i_new =  imax;
    if (i_new < -imax) i_new = -imax;

    int32_t out = p + (i_new >> 16) + d + ff;
    if (out > MOTION_OUT_MAX) {
        out = MOTION_OUT_MAX;
        if (e > 0) i_new = s_integ;       /* saturated & still pushing: freeze */
    } else if (out < -MOTION_OUT_MAX) {
        out = -MOTION_OUT_MAX;
        if (e < 0) i_new = s_integ;
    }
    s_integ = i_new;

    s_out = (int16_t)out;
    motor_set(s_out);
    return 1;
}