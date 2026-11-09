/*
 * motor.h - DRV8220 brushed-DC motor driver (VI XU LY board), bare-metal
 *
 *   PD4 -> nSLEEP
 *   PD5 -> IN1/PH
 *   PD6 -> IN2/EN
 *   JP1 OPEN     -> MODE low  -> PWM (IN1/IN2) mode  [default]
 *   JP1 SOLDERED -> MODE high -> PH/EN mode          -> -DMOTOR_MODE_PH_EN
 *
 * PWM is generated in SOFTWARE by Timer2 interrupts (overflow = pin high,
 * compare match = pin low). Timer0 (millis) is not touched at all.
 * Timer2 output pins (PB3, PD3 = ENC_B) are NOT connected: COM2x stay 0.
 *
 * PWM frequency is set by MOTOR_T2_CS in motor.c (default 7.8 kHz).
 */
#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

#define MOTOR_SLP_BIT   4
#define MOTOR_IN1_BIT   5
#define MOTOR_IN2_BIT   6

#ifndef MOTOR_INVERT
#define MOTOR_INVERT    0
#endif

#define MOTOR_DUTY_MAX  255
#define MOTOR_WAKE_US   100     /* datasheet tWAKE = 65 us */

#ifdef __cplusplus
extern "C" {
#endif

void    motor_init(void);          /* initialize with driver asleep */
void    motor_enable(uint8_t on);   /* 1 = awake, 0 = sleep (Hi-Z) */
void    motor_set(int16_t duty);    /* -255..255, 0 = brake        */
void    motor_brake(void);
void    motor_coast(void);
int16_t motor_get(void);

#ifdef __cplusplus
}
#endif

#endif
