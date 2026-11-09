#include "st7735_mini.h"
#include "adc.h"
#include "button.h"
#include "motor.h"
#include "string.h"
#include <Arduino.h>

static uint8_t duty = 0;
static volatile uint8_t uartDuty = 0;
static uint16_t uartValue = 0;
static uint8_t uartDigits = 0, uartInvalid = 0;

/* USART0 owns RX interrupt: do not use Arduino Serial at the same time.
 * Receive 1..3 decimal digits, 0..255, terminated by CR, LF or CRLF.
 * An invalid/overflowed line never changes the previous command.
 */
ISR(USART_RX_vect)
{
    const uint8_t status = UCSR0A; /* Read errors BEFORE reading UDR0. */
    const uint8_t c = UDR0;
    if (status & (_BV(FE0) | _BV(DOR0) | _BV(UPE0))) {
        uartInvalid = 1;
        return;
    }
    if (c == '\r' || c == '\n') {
        if (uartDigits && !uartInvalid) uartDuty = (uint8_t)uartValue;
        uartValue = 0;
        uartDigits = uartInvalid = 0;
    } else if (c >= '0' && c <= '9' && !uartInvalid) {
        uartValue = uartValue * 10U + (uint8_t)(c - '0');
        if (++uartDigits > 3 || uartValue > 255) uartInvalid = 1;
    } else {
        uartInvalid = 1;
    }
}

static void uartInit(void)
{
    const uint16_t ubrr = (F_CPU + 8UL * 9600UL) / (16UL * 9600UL) - 1UL;
    PRR &= (uint8_t)~_BV(PRUSART0);
    UCSR0B = 0;
    UCSR0A = 0; /* Normal speed, U2X0 = 0. */
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8 data, no parity, 1 stop. */
    uartDuty = 0;
    uartValue = 0;
    uartDigits = uartInvalid = 0;
    UCSR0B = _BV(RXEN0) | _BV(RXCIE0);
}

/* Equal-length strings overwrite each other completely (bg colour is drawn). */
static void showMode(void)
{
    const char *text;
    uint16_t color;
    const uint8_t mode = button_is_selecting() ? button_get_selected_mode() : button_get_mode();
    if (mode == BUTTON_MODE_UART) {
        text = "Mode: UART";
        color = TFT_CYAN;
    } else {
        text = "Mode: Potentiometer";
        color = TFT_YELLOW;
    }
    tft_fill_rect(0, 0, TFT_W, 8, TFT_BLACK);
    uint8_t textWidth = strlen(text) * 6;
    uint8_t x = (TFT_W - textWidth) / 2;

    tft_text(x, 0, text, color, TFT_BLACK, 1);
    tft_puts_P(0, 12, "                     ", TFT_WHITE, TFT_BLACK, 1);
    if (button_is_selecting()) tft_puts_P(0, 12, "SELECT: hold RUN=OK", TFT_WHITE, TFT_BLACK, 1);
}

static void showState(void)
{
    if (running == BUTTON_RUNNING) tft_puts_P(0, 80, "RUNNING", TFT_GREEN, TFT_BLACK, 2);
    else         tft_puts_P(0, 80, "STOP   ", TFT_RED,   TFT_BLACK, 2);
}

static void showDuty(void)
{
    tft_puts_P(0, 24, "PWM (0-255) =", TFT_WHITE, TFT_BLACK, 1);
    tft_int(0, 36, duty, 3, TFT_WHITE, TFT_BLACK, 3);
}

void setup(void)
{
    motor_init();
    button_init();
    adc_init();
    uartInit();
    tft_init();
    tft_clear(TFT_BLACK);
    showMode();
    showDuty();
    showState();
    tft_backlight(1);
}

void loop(void)
{
    button_update();
    duty = 0;
    if (running == BUTTON_RUNNING) {
        duty = button_get_mode() == BUTTON_MODE_UART ? uartDuty : (uint8_t)(adc_read(0) >> 2);
        /* Only issue a command if changed; STOP must never wake the driver. */
        if (motor_get() != duty) motor_set(duty);
    }

    /* Refresh one display region per tick; no delay blocking button scans. */
    static uint32_t lastDisplay = 0;
    static uint8_t region = 0;
    const uint32_t now = millis();
    if ((uint32_t)(now - lastDisplay) >= 80UL) {
        lastDisplay = now;
        if (region == 0) showMode();
        else if (region == 1) showState();
        else showDuty();
        region = (uint8_t)((region + 1) % 3);
    }
}
