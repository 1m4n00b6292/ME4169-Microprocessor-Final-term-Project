#include "button.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "motor.h"

#ifndef F_CPU
#error "button: define F_CPU to match the actual CPU clock"
#endif
#define BUTTON_TIMER_COUNTS (F_CPU / 64UL / 1000UL)
#if (F_CPU % 64000UL) != 0 || BUTTON_TIMER_COUNTS < 1 || BUTTON_TIMER_COUNTS > 65536UL
#error "button: F_CPU must allow an exact 1 ms Timer1 tick at prescaler 64"
#endif
#if BUTTON_DEBOUNCE_MS < 1 || BUTTON_DEBOUNCE_MS > 1000UL
#error "button: BUTTON_DEBOUNCE_MS must be 1..1000"
#endif
#if BUTTON_LONG_PRESS_MS <= BUTTON_DEBOUNCE_MS || BUTTON_LONG_PRESS_MS > 65535UL
#error "button: BUTTON_LONG_PRESS_MS must exceed debounce and be <= 65535"
#endif
#if BUTTON_STOP_ON_SELECT != 0 && BUTTON_STOP_ON_SELECT != 1
#error "button: BUTTON_STOP_ON_SELECT must be 0 or 1"
#endif

#define RUN_MASK      _BV(PC1)
#define STOP_MASK     _BV(PC3)
#define INPUT_MASK    (RUN_MASK | STOP_MASK)
#define RUN_LED_MASK  _BV(PC2)
#define STOP_LED_MASK _BV(PC4)
#define LED_MASK      (RUN_LED_MASK | STOP_LED_MASK)

uint8_t running = BUTTON_STOPPED;

static volatile uint32_t s_ms = 0;
static uint8_t s_mode = BUTTON_MODE_POT;
static uint8_t s_selected_mode = BUTTON_MODE_POT;
static uint8_t s_selecting = 0;

struct DebouncedButton {
    uint32_t changed_at;
    uint8_t raw;
    uint8_t pressed;
};

static DebouncedButton s_run = {0, 0, 0};
static DebouncedButton s_stop = {0, 0, 0};
static uint32_t s_pressed_at = 0;
static uint32_t s_released_at = 0;
static uint8_t s_long_sent = 0;
static uint8_t s_run_blocked = 1;
static uint8_t s_release_tracking = 0;

ISR(TIMER1_COMPA_vect)
{
    ++s_ms;
}

static uint32_t clock_ms(void)
{
    uint32_t now;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { now = s_ms; }
    return now;
}

static void debounce(DebouncedButton &key, uint8_t raw, uint32_t now)
{
    if (raw != key.raw) {
        key.raw = raw;
        key.changed_at = now;
    }
    if ((uint32_t)(now - key.changed_at) >= BUTTON_DEBOUNCE_MS) {
        key.pressed = key.raw;
    }
}

static void apply_leds(void)
{
    const uint8_t on = running == BUTTON_RUNNING ? RUN_LED_MASK : STOP_LED_MASK;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        /* Bao toan PC0 (ADC), PC5 va trang thai pull-up cua cac chan khac. */
        PORTC = (uint8_t)((PORTC & (uint8_t)~LED_MASK) | on);
    }
}

static void stop_motor(void)
{
    /* Xoa PWM, keo PD4 LOW va dong bo s_awake cua motor.c.
     * Chi ghi PORTD truc tiep se lam s_awake sai khi RUN lai.
     */
    motor_enable(0);
    running = BUTTON_STOPPED;
}

void button_stop(void)
{
    stop_motor();
    s_selecting = 0;
    s_selected_mode = s_mode;
    s_run_blocked = 1;
    s_release_tracking = 0;
    s_long_sent = 0;
    apply_leds();
}

static void short_press(void)
{
    if (s_selecting) {
        s_selected_mode ^= 1U;
    } else if (running == BUTTON_STOPPED) {
        motor_enable(1); /* PD4 HIGH; motor.c cho MOTOR_WAKE_US. */
        running = BUTTON_RUNNING;
    }
}

static void long_press(void)
{
    if (s_selecting) {
        s_mode = s_selected_mode;
        s_selecting = 0;
    } else {
#if BUTTON_STOP_ON_SELECT
        stop_motor();
#endif
        s_selected_mode = s_mode;
        s_selecting = 1;
    }
}

void button_init(void)
{
    /* Goi SAU motor_init()/motion_init() de motor san sang truoc nut nhan. */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        PRR &= (uint8_t)~_BV(PRTIM1);
        TCCR1B = 0;
        TIMSK1 = 0;
        TCCR1A = 0; /* Ngat ket noi OC1A/OC1B; giu PB1/PB2 cho TFT. */
        TCCR1C = 0;

        s_ms = 0;
        s_mode = BUTTON_MODE_POT;
        s_selected_mode = BUTTON_MODE_POT;
        s_selecting = 0;
        s_run.changed_at = s_stop.changed_at = 0;
        s_run.raw = s_run.pressed = 0;
        s_stop.raw = s_stop.pressed = 0;
        s_pressed_at = s_released_at = 0;
        s_long_sent = 0;
        s_run_blocked = 1;
        s_release_tracking = 0;

        stop_motor();
        DDRD |= _BV(MOTOR_SLP_BIT);
        DDRC &= (uint8_t)~INPUT_MASK;
        PORTC &= (uint8_t)~INPUT_MASK; /* R3/R4 pull-down: TAT pull-up noi. */
        DIDR0 &= (uint8_t)~(_BV(ADC1D) | _BV(ADC3D));
        apply_leds();             /* Dat muc truoc khi chuyen thanh output. */
        DDRC |= LED_MASK;

        TCNT1 = 0;
        OCR1A = (uint16_t)(BUTTON_TIMER_COUNTS - 1UL);
        /* Cac co nay xoa bang cach ghi 1, khong dung |=. */
        TIFR1 = _BV(ICF1) | _BV(OCF1B) | _BV(OCF1A) | _BV(TOV1);
        TIMSK1 = _BV(OCIE1A);
        TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10); /* CTC, /64. */
    }
}

void button_update(void)
{
    const uint32_t now = clock_ms();
    const uint8_t pins = PINC;
    const uint8_t raw_run = (pins & RUN_MASK) ? 1U : 0U;
    const uint8_t raw_stop = (pins & STOP_MASK) ? 1U : 0U;
    const uint8_t was_run = s_run.pressed;
    const uint8_t was_stop = s_stop.pressed;

    debounce(s_run, raw_run, now);
    debounce(s_stop, raw_stop, now);

    /* STOP khong can cho nha nut va luon uu tien hon RUN. */
    if (s_stop.pressed && !was_stop) button_stop();

    /* Chan RUN ngay ca khi STOP moi bat dau chong doi.
     * Xung STOP <20 ms khong dung motor, nhung huy cu chi RUN dang do.
     */
    if (raw_stop || s_stop.pressed) {
        s_run_blocked = 1;
        s_release_tracking = 0;
        s_long_sent = 0;
    }

    if (s_run_blocked) {
        if (!raw_run && !raw_stop && !s_run.pressed && !s_stop.pressed) {
            if (!s_release_tracking) {
                s_released_at = now;
                s_release_tracking = 1;
            } else if ((uint32_t)(now - s_released_at) >= BUTTON_DEBOUNCE_MS) {
                s_run_blocked = 0;
                s_release_tracking = 0;
            }
        } else {
            s_release_tracking = 0;
        }
        apply_leds();
        return;
    }

    if (s_run.pressed && !was_run) {
        s_pressed_at = now;
        s_long_sent = 0;
    } else if (!s_run.pressed && was_run) {
        if (!s_long_sent) short_press();
        s_long_sent = 0;
    } else if (s_run.pressed && raw_run && !s_long_sent &&
               (uint32_t)(now - s_pressed_at) >= BUTTON_LONG_PRESS_MS) {
        long_press();
        s_long_sent = 1; /* Khong lap khi giu, khong phat short khi nha. */
    }

    apply_leds();
}

uint8_t button_get_mode(void) { return s_mode; }
uint8_t button_get_selected_mode(void) { return s_selected_mode; }
uint8_t button_is_selecting(void) { return s_selecting; }
