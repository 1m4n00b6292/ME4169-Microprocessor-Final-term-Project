#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include "motor.h"

/*
 * Timer2 clock (fast PWM, TOP = 255, PWM freq = F_CPU / prescaler / 256):
 *   _BV(CS21)             prescaler   8 -> 7.8  kHz   (default)
 *   _BV(CS21)|_BV(CS20)   prescaler  32 -> 1.95 kHz
 *   _BV(CS22)             prescaler  64 -> 0.98 kHz
 */
#ifndef MOTOR_T2_CS
#define MOTOR_T2_CS  _BV(CS21)
#endif

#define IN1_MASK  _BV(MOTOR_IN1_BIT)    /* PD5 <-> OCR2A / COMPA */
#define IN2_MASK  _BV(MOTOR_IN2_BIT)    /* PD6 <-> OCR2B / COMPB */

static int16_t           s_duty  = 0;
static uint8_t           s_awake = 0;
static volatile uint8_t  s_mask  = 0;   /* pins currently being PWM'd */

/* ---- software PWM ISRs ---- */
ISR(TIMER2_OVF_vect)   { PORTD |=  s_mask; }    /* start of period: raise active pins */
ISR(TIMER2_COMPA_vect) { PORTD &= ~IN1_MASK; }  /* IN1 duty finished */
ISR(TIMER2_COMPB_vect) { PORTD &= ~IN2_MASK; }  /* IN2 duty finished */

/*
 * d = 0 / 255 : PWM off for that pin, plain constant level (also what the
 *               PH pin uses in PH/EN mode).
 * d = 1..254  : PWM with duty ~ d/256.
 * Note: very small duties (~1-5) are limited by ISR latency (a few us), so
 * the real on-time bottoms out around 2 %.
 */
static void in1_pwm(uint8_t d)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (d == 0 || d == 255) {
            TIMSK2 &= ~_BV(OCIE2A);
            s_mask &= ~IN1_MASK;
            if (d) PORTD |=  IN1_MASK;
            else   PORTD &= ~IN1_MASK;
        } else {
            OCR2A = d;
            if (!(s_mask & IN1_MASK)) {
                TIFR2   = _BV(OCF2A);           /* drop stale match flag */
                s_mask |= IN1_MASK;
                TIMSK2 |= _BV(OCIE2A);
            }
        }
    }
}

static void in2_pwm(uint8_t d)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (d == 0 || d == 255) {
            TIMSK2 &= ~_BV(OCIE2B);
            s_mask &= ~IN2_MASK;
            if (d) PORTD |=  IN2_MASK;
            else   PORTD &= ~IN2_MASK;
        } else {
            OCR2B = d;
            if (!(s_mask & IN2_MASK)) {
                TIFR2   = _BV(OCF2B);
                s_mask |= IN2_MASK;
                TIMSK2 |= _BV(OCIE2B);
            }
        }
    }
}

void motor_enable(uint8_t on)
{
    if (on) {
        if (!s_awake) {
            PORTD |= _BV(MOTOR_SLP_BIT);
            s_awake = 1;
            _delay_us(MOTOR_WAKE_US);
        }
    } else {
        in1_pwm(0);                             /* inputs low before sleep */
        in2_pwm(0);
        PORTD &= ~_BV(MOTOR_SLP_BIT);
        s_awake = 0;
        s_duty  = 0;
    }
}

void motor_init(void)
{
    PORTD &= ~(_BV(MOTOR_SLP_BIT) | IN1_MASK | IN2_MASK);
    DDRD  |=  (_BV(MOTOR_SLP_BIT) | IN1_MASK | IN2_MASK);

    /* Timer2: fast PWM, TOP=0xFF, outputs NOT connected (COM2x = 0:
       PB3 is MOSI and PD3 is ENC_B), overflow interrupt on. */
    TCCR2A = _BV(WGM21) | _BV(WGM20);
    TCCR2B = MOTOR_T2_CS;
    OCR2A  = 0;
    OCR2B  = 0;
    s_mask = 0;
    TIFR2  = _BV(TOV2) | _BV(OCF2A) | _BV(OCF2B);
    TIMSK2 = _BV(TOIE2);

    s_awake = 0;
    s_duty  = 0;
    motor_enable(0);
}

void motor_set(int16_t duty)
{
    if (duty >  MOTOR_DUTY_MAX) duty =  MOTOR_DUTY_MAX;
    if (duty < -MOTOR_DUTY_MAX) duty = -MOTOR_DUTY_MAX;
    s_duty = duty;
#if MOTOR_INVERT
    duty = -duty;
#endif
    if (!s_awake) motor_enable(1);

    uint8_t mag = (duty < 0) ? (uint8_t)(-duty) : (uint8_t)duty;

#ifdef MOTOR_MODE_PH_EN
    /* PH = direction (constant level), EN = PWM. EN low = brake (auto-sleeps after ~2 ms) */
    in1_pwm(duty >= 0 ? 255 : 0);
    in2_pwm(mag);
#else
    /* PWM mode, slow decay: one input held high, the other PWM'd with inverted
       duty -> off-time is brake (1,1), not coast. Held-high input is set first
       so direction changes pass through brake. */
    if (duty >= 0) { in1_pwm(255); in2_pwm(255 - mag); }
    else           { in2_pwm(255); in1_pwm(255 - mag); }
#endif
}

void motor_brake(void)
{
    s_duty = 0;
    if (!s_awake) motor_enable(1);
#ifdef MOTOR_MODE_PH_EN
    in2_pwm(0);
#else
    in1_pwm(255);
    in2_pwm(255);
#endif
}

void motor_coast(void)
{
#ifdef MOTOR_MODE_PH_EN
    motor_enable(0);                            /* only Hi-Z path in PH/EN mode */
#else
    in1_pwm(0);
    in2_pwm(0);
    s_duty = 0;
#endif
}

int16_t motor_get(void) { return s_duty; }
