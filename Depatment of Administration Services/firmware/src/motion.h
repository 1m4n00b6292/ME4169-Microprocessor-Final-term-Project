/*
 * motion.h - closed-loop velocity control (fixed-point PID, no float at runtime)
 *
 * Input : measured velocity from the encoder library, as int16 in ANY unit
 *         (RPM, counts/s, ...). Gains are tuned in that unit.
 * Output: signed duty -255..255 sent to motor_set().
 *
 * Call motion_update(measured) as often as you like from loop(); it only runs
 * a PID step every motion_set_period() ms. The encoder velocity should be
 * refreshed at least as often as that period.
 *
 * Gains are Q8 fixed point (value*256). Use MOTION_GAIN(x) for constants:
 *     motion_set_gains(MOTION_GAIN(0.8), MOTION_GAIN(4.0), MOTION_GAIN(0.01));
 */
#ifndef MOTION_H
#define MOTION_H

#include <stdint.h>
#include "config.h"

#define MOTION_GAIN(x)      ((int16_t)((x) * 256.0f + 0.5f))

void    motion_init(void);                              /* calls motor_init() */
void    motion_set_gains(int16_t kp, int16_t ki, int16_t kd);   /* Q8 */
void    motion_set_feedforward(int16_t kff);            /* Q8 duty per velocity unit, 0 = off */
void    motion_set_period(uint8_t ms);                  /* PID step period    */
void    motion_set_accel(uint16_t units_per_s);         /* setpoint ramp, 0 = off */

void    motion_set_target(int16_t velocity);            /* signed velocity setpoint */
int16_t motion_get_target(void);                        /* final target           */
int16_t motion_get_setpoint(void);                      /* ramped setpoint in use */
int16_t motion_get_output(void);                        /* last duty sent         */

void    motion_start(void);                             /* enable loop            */
void    motion_stop(void);                              /* output 0, brake, reset PID */
uint8_t motion_is_running(void);

/* returns 1 when a PID step was executed on this call */
uint8_t motion_update(int16_t measured);

#endif