/*
 * config.h - central configuration for the VI XU LY board (ATmega88PA)
 *
 * Every tunable figure of every library lives here. Libraries just do
 *     #include "config.h"
 * at the top. Names that already existed inside the libraries (TFT_*, MOTOR_*,
 * ENC_INVERT, MOTION_*) are kept, so the old #ifndef defaults become dead code.
 *
 * Works from both C and C++ files.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <avr/io.h>

/* ======================================================================
 * 1. SYSTEM
 * ====================================================================== */
#ifndef F_CPU
#define F_CPU 16000000UL            /* 16 MHz crystal (Y1) - normally set by platformio.ini */
#endif

#define UART_BAUD   115200UL        /* used by uart.c (RX only, U2X mode) */

/* ======================================================================
 * 2. PIN MAP
 *
 *   PB0 TFT_RST    PB1 TFT_BL     PB2 TFT_CS     PB3 TFT_MOSI(fixed)  PB5 TFT_SCK(fixed)
 *   PD7 TFT_DC
 *   PD2 ENC_A (INT0, fixed)       PD3 ENC_B (INT1, fixed)
 *   PD4 MOTOR_SLEEP   PD5 MOTOR_IN1/PH (Timer2 COMPA)   PD6 MOTOR_IN2/EN (Timer2 COMPB)
 *   PC0 POT (ADC0)
 *
 * Ports are fixed in the code (PORTB / PORTD); only the bit numbers are
 * configurable. Pins marked "fixed" or tied to a Timer2/INT vector cannot move
 * without editing the ISR / register code in the library.
 * ====================================================================== */

/* ---- TFT (PORTB, DC on PORTD) ---- */
#define TFT_RST_PIN     PB0
#define TFT_BL_PIN      PB1
#define TFT_CS_PIN      PB2
#define TFT_MOSI_PIN    PB3         /* fixed by hardware SPI */
#define TFT_SCK_PIN     PB5         /* fixed by hardware SPI */
#define TFT_DC_PIN      PD7         /* PORTD */

/* ---- Encoder (PORTD) - tied to INT0 / INT1 ---- */
#define ENC_A_BIT       2
#define ENC_B_BIT       3

/* ---- Motor driver DRV8220 (PORTD) ---- */
#define MOTOR_SLP_BIT   4
#define MOTOR_IN1_BIT   5           /* tied to Timer2 COMPA (OCR2A) */
#define MOTOR_IN2_BIT   6           /* tied to Timer2 COMPB (OCR2B) */

/* ======================================================================
 * 3. TFT  (st7735_mini)
 * ====================================================================== */
#define TFT_LANDSCAPE       1       /* 0 = portrait 128x160, 1 = landscape 160x128 */
#define TFT_COL_OFFSET      2
#define TFT_ROW_OFFSET      1
#define TFT_BL_ACTIVE_HIGH  1       /* 0 if the backlight is active-low */

/* SPI clock divider: 2, 4, 8, 16, 32, 64 or 128  (fosc/DIV; 2 = 8 MHz @ 16 MHz) */
#define TFT_SPI_DIV         2

#if   TFT_SPI_DIV == 2
  #define TFT_SPCR_RATE   0
  #define TFT_SPSR_RATE   _BV(SPI2X)
#elif TFT_SPI_DIV == 4
  #define TFT_SPCR_RATE   0
  #define TFT_SPSR_RATE   0
#elif TFT_SPI_DIV == 8
  #define TFT_SPCR_RATE   _BV(SPR0)
  #define TFT_SPSR_RATE   _BV(SPI2X)
#elif TFT_SPI_DIV == 16
  #define TFT_SPCR_RATE   _BV(SPR0)
  #define TFT_SPSR_RATE   0
#elif TFT_SPI_DIV == 32
  #define TFT_SPCR_RATE   _BV(SPR1)
  #define TFT_SPSR_RATE   _BV(SPI2X)
#elif TFT_SPI_DIV == 64
  #define TFT_SPCR_RATE   _BV(SPR1)
  #define TFT_SPSR_RATE   0
#elif TFT_SPI_DIV == 128
  #define TFT_SPCR_RATE   (_BV(SPR1) | _BV(SPR0))
  #define TFT_SPSR_RATE   0
#else
  #error "TFT_SPI_DIV must be 2, 4, 8, 16, 32, 64 or 128"
#endif

/* ======================================================================
 * 4. ADC  (adc)
 * ====================================================================== */
#define ADC_POT_CHANNEL     0       /* PC0 / ADC0 */

/* Reference: 0 = AREF, 1 = AVCC (+5VA via L1/C), 2 = internal 1.1 V */
#define ADC_REFERENCE       1
#if   ADC_REFERENCE == 0
  #define ADC_REF_BITS    0
#elif ADC_REFERENCE == 1
  #define ADC_REF_BITS    _BV(REFS0)
#elif ADC_REFERENCE == 2
  #define ADC_REF_BITS    (_BV(REFS1) | _BV(REFS0))
#else
  #error "ADC_REFERENCE must be 0, 1 or 2"
#endif

/* ADC clock prescaler: 2, 4, 8, 16, 32, 64 or 128 (keep ADC clock 50..200 kHz) */
#define ADC_PRESCALER       128
#if   ADC_PRESCALER == 2
  #define ADC_PS_BITS     _BV(ADPS0)
#elif ADC_PRESCALER == 4
  #define ADC_PS_BITS     _BV(ADPS1)
#elif ADC_PRESCALER == 8
  #define ADC_PS_BITS     (_BV(ADPS1) | _BV(ADPS0))
#elif ADC_PRESCALER == 16
  #define ADC_PS_BITS     _BV(ADPS2)
#elif ADC_PRESCALER == 32
  #define ADC_PS_BITS     (_BV(ADPS2) | _BV(ADPS0))
#elif ADC_PRESCALER == 64
  #define ADC_PS_BITS     (_BV(ADPS2) | _BV(ADPS1))
#elif ADC_PRESCALER == 128
  #define ADC_PS_BITS     (_BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0))
#else
  #error "ADC_PRESCALER must be 2, 4, 8, 16, 32, 64 or 128"
#endif

#define ADC_MAX             1023    /* 10-bit */

/* ======================================================================
 * 5. ENCODER  (encoder_mini)
 * ====================================================================== */
#define ENC_INVERT            /* uncomment to swap the counting direction */
#define ENC_PULLUPS         1       /* internal pull-ups on A/B */
#define ENC_WINDOW_MS       50      /* velocity refresh window */
#define ENC_FILTER_N            4
#define ENC_FILTER_SAMPLE_HZ    80000UL
#if ENC_FILTER_N > 1 && ((F_CPU / ENC_FILTER_SAMPLE_HZ) - 1) > 65535UL
  #error "ENC_FILTER_SAMPLE_HZ too low for Timer1 (min ~245 Hz at 16 MHz)"
#endif
#define ENC_PPR             20     /* TODO: set to YOUR encoder's pulses per revolution */
#define ENC_CPR             (ENC_PPR * 4)   /* x4 decoding */

/* ======================================================================
 * 6. MOTOR  (motor)
 * ====================================================================== */
#define MOTOR_INVERT        0       /* 1 = flip rotation direction */
/* #define MOTOR_MODE_PH_EN */      /* uncomment when JP1 is SOLDERED (PH/EN mode) */
#define MOTOR_DUTY_MAX      255     /* do not exceed 255 (8-bit PWM) */
#define MOTOR_WAKE_US       100     /* DRV8220 tWAKE = 65 us */

/* Software-PWM frequency = F_CPU / MOTOR_T2_PRESCALER / 256
 * Allowed: 1, 8, 32, 64, 128, 256, 1024    (8 -> 7.8 kHz @ 16 MHz)
 * Higher frequency = more ISR load. */
#define MOTOR_T2_PRESCALER  32
#if   MOTOR_T2_PRESCALER == 1
  #define MOTOR_T2_CS     _BV(CS20)
#elif MOTOR_T2_PRESCALER == 8
  #define MOTOR_T2_CS     _BV(CS21)
#elif MOTOR_T2_PRESCALER == 32
  #define MOTOR_T2_CS     (_BV(CS21) | _BV(CS20))
#elif MOTOR_T2_PRESCALER == 64
  #define MOTOR_T2_CS     _BV(CS22)
#elif MOTOR_T2_PRESCALER == 128
  #define MOTOR_T2_CS     (_BV(CS22) | _BV(CS20))
#elif MOTOR_T2_PRESCALER == 256
  #define MOTOR_T2_CS     (_BV(CS22) | _BV(CS21))
#elif MOTOR_T2_PRESCALER == 1024
  #define MOTOR_T2_CS     (_BV(CS22) | _BV(CS21) | _BV(CS20))
#else
  #error "MOTOR_T2_PRESCALER must be 1, 8, 32, 64, 128, 256 or 1024"
#endif

/* ======================================================================
 * 7. MOTION / PID  (motion)
 *    Gains are plain numbers here; convert in code with MOTION_GAIN(x):
 *      motion_set_gains(MOTION_GAIN(MOTION_KP), MOTION_GAIN(MOTION_KI), MOTION_GAIN(MOTION_KD));
 * ====================================================================== */
#define MOTION_PERIOD_MS        50      /* PID step period (= ENC_WINDOW_MS, so every step sees a fresh velocity) */
#define MOTION_OUT_MAX          255     /* output clamp (<= MOTOR_DUTY_MAX) */
#define MOTION_D_FILTER_SHIFT   2       /* derivative low-pass: 1/4 new, 3/4 old */

/* Starting gains - PLACEHOLDERS, tune on the real motor. Unit = encoder velocity unit. */
#define MOTION_KP               0.03f
#define MOTION_KI               0.2f
#define MOTION_KD               0
#define MOTION_KFF              0.085f    /* duty per velocity unit, 0 = off */
#define MOTION_ACCEL            6000       /* setpoint ramp (units/s), 0 = off */
#define MOTION_TARGET_MAX       255     /* target mapped to full pot travel (0..255) */

/* ======================================================================
 * 8. APPLICATION  (main.cpp)
 * ====================================================================== */
#define APP_LOOP_DELAY_MS       250

/* layout (pixels) */
#define APP_MODE_Y              0
#define APP_VEL_LABEL_Y         24
#define APP_VEL_VALUE_Y         36
#define APP_STATE_Y             80

/* text sizes */
#define APP_MODE_SIZE           1
#define APP_VEL_LABEL_SIZE      1
#define APP_VEL_VALUE_SIZE      3
#define APP_STATE_SIZE          2
#define APP_VEL_DIGITS          6       /* padded width of the velocity number */

/* colours (see TFT_* in st7735_mini.h) */
#define APP_COLOR_BG            TFT_BLACK
#define APP_COLOR_MODE_UART     TFT_CYAN
#define APP_COLOR_MODE_POT      TFT_YELLOW
#define APP_COLOR_LABEL         TFT_WHITE
#define APP_COLOR_VALUE         TFT_WHITE
#define APP_COLOR_RUN           TFT_GREEN
#define APP_COLOR_STOP          TFT_RED

/* strings (state strings must stay the same length so they overwrite each other) */
#define APP_STR_MODE_UART       "Mode: UART"
#define APP_STR_MODE_POT        "Mode: Potentiometer"
#define APP_STR_VELOCITY        "Velocity ="
#define APP_STR_RUNNING         "RUNNING"
#define APP_STR_STOP            "STOP   "

/* ======================================================================
 * 9. BUTTONS / LEDS / UART  (button, uart, app)
 * ====================================================================== */
#define BTN_RUN_BIT         1
#define BTN_STOP_BIT        3
#define LED_RUN_BIT         2
#define LED_STOP_BIT        4

#define BTN_ACTIVE_HIGH     1       /* switch to +5V, 10K pull-down on the board */
#define BTN_DEBOUNCE_MS     20      /* signal must be stable this long */
#define BTN_LONG_MS         2000    /* hold time for a long press */

#define UART_RX_BUF_SIZE    32      /* power of 2, max 256 */

/* ======================================================================
 * 10. APPLICATION LOGIC
 * ====================================================================== */
#define APP_POT_CENTER          512     /* ADC value = stop                          */
#define APP_POT_DEADBAND        15      /* +/- counts around center that still = 0   */
                                        /* pot > center -> CCW (negative target)     */
                                        /* pot < center -> CW  (positive target)     */
#define APP_POT_INVERT          0       /* 1 = swap pot direction (above centre = CW) */
#define APP_MAX_RPM             2700     /* closed loop: full pot / UART +-255 = this RPM.*/
#define APP_OPEN_LOOP           0       /* 1 = target (0..255) goes straight to motor_set(), no PID
                                           0 = closed loop: target is compared with the encoder */
#define APP_VELOCITY_RPM        1       /* 1 = velocity in RPM, 0 = encoder counts/s */
#define APP_DISPLAY_MS          100     /* velocity redraw period */
#define APP_BACKLIGHT_ON        0       /* value passed to tft_backlight() after init */
#define APP_TARGET_Y            64
#define APP_TARGET_X            54      /* "Target =" is 8 chars * 6 px = 48 px */
#if !APP_OPEN_LOOP && !APP_VELOCITY_RPM
  #error "closed loop needs APP_VELOCITY_RPM 1 (APP_MAX_RPM is in RPM)"
#endif
#define APP_STR_TARGET          "Target ="
#define APP_STR_SELECT          "SELECT "
#define APP_COLOR_SELECT        TFT_ORANGE

#endif /* CONFIG_H */