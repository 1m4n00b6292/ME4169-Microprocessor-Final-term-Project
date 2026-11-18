/*
 * app.cpp - integrates all libraries and applies VIXULY_Logic.txt
 * (C++ because EncoderMini is a class; the C libraries are wrapped in extern "C")
 */
#include <Arduino.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "config.h"
#include "app.h"
#include "adc.h"
#include "st7735_mini.h"
#include "button.h"
#include "uart.h"
#include "encoder_mini.h"
extern "C" {
#include "motor.h"
#include "motion.h"
}

uint8_t running = 0;

/* ------------------------------------------------------------------ */
enum State : uint8_t { ST_STOP, ST_RUN, ST_SELECT };
enum Mode  : uint8_t { MODE_POT = 0, MODE_UART = 1 };

static State    s_state = ST_STOP;
static uint8_t  s_mode  = MODE_POT;      /* active mode              */
static uint8_t  s_sel   = MODE_POT;      /* mode shown while selecting */

static button_t s_btnRun, s_btnStop;
static int16_t  s_meas  = 0;             /* measured velocity        */
static uint32_t s_dispT = 0;

/* what is currently on the screen (sentinel = "not drawn yet") */
static uint8_t  s_dMode  = 0xFF;
static uint8_t  s_dState = 0xFF;
static int16_t  s_dVel   = INT16_MIN;
static int16_t  s_dTgt   = INT16_MIN;

/* ------------------------------------------------------------------ display */
static void drawMode(void)
{
    uint8_t m = (s_state == ST_SELECT) ? s_sel : s_mode;
    if (m == s_dMode) return;
    s_dMode = m;

    const char *t = m ? PSTR(APP_STR_MODE_UART) : PSTR(APP_STR_MODE_POT);
    uint16_t    c = m ? APP_COLOR_MODE_UART     : APP_COLOR_MODE_POT;

    tft_fill_rect(0, APP_MODE_Y, TFT_W, 8 * APP_MODE_SIZE, APP_COLOR_BG);
    uint8_t w = (uint8_t)(strlen_P(t) * 6 * APP_MODE_SIZE);
    tft_text_P((uint8_t)((TFT_W - w) / 2), APP_MODE_Y, t, c, APP_COLOR_BG, APP_MODE_SIZE);
}

static void drawState(void)
{
    if (s_state == s_dState) return;
    s_dState = s_state;

    switch (s_state) {
    case ST_RUN:
        tft_puts_P(0, APP_STATE_Y, APP_STR_RUNNING, APP_COLOR_RUN,    APP_COLOR_BG, APP_STATE_SIZE);
        break;
    case ST_SELECT:
        tft_puts_P(0, APP_STATE_Y, APP_STR_SELECT,  APP_COLOR_SELECT, APP_COLOR_BG, APP_STATE_SIZE);
        break;
    default:
        tft_puts_P(0, APP_STATE_Y, APP_STR_STOP,    APP_COLOR_STOP,   APP_COLOR_BG, APP_STATE_SIZE);
        break;
    }
}

static void drawVelocity(void)
{
    if (s_dVel == INT16_MIN)
        tft_puts_P(0, APP_VEL_LABEL_Y, APP_STR_VELOCITY, APP_COLOR_LABEL, APP_COLOR_BG, APP_VEL_LABEL_SIZE);
    if (s_meas != s_dVel) {
        s_dVel = s_meas;
        tft_int(0, APP_VEL_VALUE_Y, s_meas, APP_VEL_DIGITS,
                APP_COLOR_VALUE, APP_COLOR_BG, APP_VEL_VALUE_SIZE);
    }

    int16_t tg = motion_get_target();
    if (s_dTgt == INT16_MIN)
        tft_puts_P(0, APP_TARGET_Y, APP_STR_TARGET, APP_COLOR_LABEL, APP_COLOR_BG, 1);
    if (tg != s_dTgt) {
        s_dTgt = tg;
        tft_int(APP_TARGET_X, APP_TARGET_Y, tg, APP_VEL_DIGITS,
                APP_COLOR_VALUE, APP_COLOR_BG, 1);
    }
}

/* ------------------------------------------------------------------ LEDs / state */
static void setLeds(void)
{
    if (s_state == ST_RUN) {
        PORTC |=  _BV(LED_RUN_BIT);
        PORTC &= ~_BV(LED_STOP_BIT);
    } else {
        PORTC &= ~_BV(LED_RUN_BIT);
        PORTC |=  _BV(LED_STOP_BIT);
    }
}

static void goStop(void)
{
    motion_stop();                 /* PID reset, output 0, brake            */
    motion_set_target(0);
    motor_enable(0);               /* PD4 (SLEEP) low -> driver off         */
    running = 0;
    s_sel   = s_mode;              /* cancels an unfinished mode selection  */
    s_state = ST_STOP;
    setLeds();
}

static void goRun(void)
{
    if (s_mode == MODE_UART) uart_flush();     /* start from a clean line   */
    motion_set_target(0);
    motor_enable(1);
    motion_start();
    running = 1;
    s_state = ST_RUN;
    setLeds();
}

/* ------------------------------------------------------------------ buttons */
static void handleButtons(void)
{
    btn_event_t rb = btn_update(&s_btnRun);
    btn_event_t sb = btn_update(&s_btnStop);

    if (sb != BTN_EVT_NONE) {                  /* STOP always wins          */
        goStop();
        return;
    }

    switch (s_state) {
    case ST_STOP:
        if (rb == BTN_EVT_SHORT) {
            goRun();
        } else if (rb == BTN_EVT_LONG) {
            s_sel   = s_mode;
            s_state = ST_SELECT;
        }
        break;

    case ST_SELECT:
        if (rb == BTN_EVT_SHORT) {
            s_sel ^= 1;                        /* swap the two modes        */
        } else if (rb == BTN_EVT_LONG) {
            s_mode  = s_sel;                   /* confirm, back to STOP     */
            s_state = ST_STOP;
        }
        break;

    case ST_RUN:                               /* RUN button does nothing   */
    default:
        break;
    }
}

/* ------------------------------------------------------------------ setpoint */
/* 0..1023 -> -MAX..+MAX, centre (with dead band) = 0.
 * Pot above centre = CCW = negative, below centre = CW = positive. */
static int16_t potTarget(uint16_t raw)
{
    int32_t d   = (int32_t)raw - APP_POT_CENTER;
    int32_t mag = (d < 0) ? -d : d;
    if (mag <= APP_POT_DEADBAND) return 0;

    int32_t span = (ADC_MAX - APP_POT_CENTER) - APP_POT_DEADBAND;   /* 511 - dead band */
    int32_t t    = (mag - APP_POT_DEADBAND) * MOTION_TARGET_MAX / span;
    if (t > MOTION_TARGET_MAX) t = MOTION_TARGET_MAX;
#if APP_POT_INVERT
    return (int16_t)((d > 0) ? t : -t);
#else
    return (int16_t)((d > 0) ? -t : t);
#endif
}

static void applyTarget(int16_t t)
{
#if APP_OPEN_LOOP
    motion_set_target(t);              /* target 0..255 = duty, no PID       */
    motor_set(t);
#else
    /* -255..255 -> -APP_MAX_RPM..+APP_MAX_RPM, PID does the rest */
    motion_set_target((int16_t)(((int32_t)t * APP_MAX_RPM) / MOTION_TARGET_MAX));
#endif
}

static void updateTarget(void)
{
    if (s_state != ST_RUN) return;

    if (s_mode == MODE_POT) {
        applyTarget(potTarget(adc_read(ADC_POT_CHANNEL)));
    } else {
        int16_t v;
        if (uart_poll_int(&v)) {
            if (v >  MOTION_TARGET_MAX) v =  MOTION_TARGET_MAX;
            if (v < -MOTION_TARGET_MAX) v = -MOTION_TARGET_MAX;
            applyTarget(v);
        }
    }
}

static void updateVelocity(void)
{
    if (!enc.update()) return;
#if APP_VELOCITY_RPM
    int32_t v = enc.rpm();
#else
    int32_t v = enc.velocity();
#endif
    if (v >  32000L) v =  32000L;
    if (v < -32000L) v = -32000L;
    s_meas = (int16_t)v;
}

/* ------------------------------------------------------------------ public */
void app_init(void)
{
    /* LEDs + buttons (PORTC) */
    DDRC |= _BV(LED_RUN_BIT) | _BV(LED_STOP_BIT);
    btn_init(&s_btnRun,  BTN_RUN_BIT);
    btn_init(&s_btnStop, BTN_STOP_BIT);

    /* peripherals */
    tft_init();
    tft_clear(APP_COLOR_BG);
    adc_init();
    uart_init();
    enc.begin(ENC_PULLUPS);
    enc.setCountsPerRev(ENC_CPR);

    /* motor + PID */
    motion_init();
    motion_set_gains(MOTION_GAIN(MOTION_KP), MOTION_GAIN(MOTION_KI), MOTION_GAIN(MOTION_KD));
    motion_set_feedforward(MOTION_GAIN(MOTION_KFF));
    motion_set_period(MOTION_PERIOD_MS);
    motion_set_accel(MOTION_ACCEL);

    /* power-up state = STOP */
    goStop();
    drawMode();
    drawState();
    drawVelocity();
    tft_backlight(APP_BACKLIGHT_ON);
}

void app_update(void)
{
    handleButtons();
    updateVelocity();
    updateTarget();
#if !APP_OPEN_LOOP
    motion_update(s_meas);          /* runs a PID step every MOTION_PERIOD_MS */
#endif

    drawMode();
    drawState();
    if ((uint32_t)(millis() - s_dispT) >= APP_DISPLAY_MS) {
        s_dispT = millis();
        drawVelocity();
    }
}