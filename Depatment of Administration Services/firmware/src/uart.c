#include <avr/io.h>
#include <avr/interrupt.h>
#include "uart.h"
#include "config.h"

#define RX_MASK   (UART_RX_BUF_SIZE - 1)
/* U2X mode: UBRR = F_CPU / (8 * baud) - 1, rounded. 16 MHz / 115200 -> 16 (2.1 % error) */
#define UART_UBRR ((uint16_t)(((F_CPU) + 4UL * UART_BAUD) / (8UL * UART_BAUD) - 1UL))

static volatile uint8_t s_rx[UART_RX_BUF_SIZE];
static volatile uint8_t s_head = 0, s_tail = 0;

/* line parser state (kept across calls so a line can arrive in pieces) */
static int32_t s_val    = 0;
static uint8_t s_neg    = 0;
static uint8_t s_digits = 0;

ISR(USART_RX_vect)
{
    uint8_t d = UDR0;
    uint8_t n = (s_head + 1) & RX_MASK;
    if (n != s_tail) {                 /* drop byte if the buffer is full */
        s_rx[s_head] = d;
        s_head = n;
    }
}

void uart_init(void)
{
    UBRR0  = UART_UBRR;
    UCSR0A = _BV(U2X0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);          /* 8N1 */
    UCSR0B = _BV(RXEN0) | _BV(RXCIE0);           /* RX + RX interrupt, TX off */
}

void uart_flush(void)
{
    s_tail   = s_head;
    s_val    = 0;
    s_neg    = 0;
    s_digits = 0;
}

uint8_t uart_poll_int(int16_t *value)
{
    while (s_tail != s_head) {
        uint8_t c = s_rx[s_tail];
        s_tail = (s_tail + 1) & RX_MASK;

        if (c >= '0' && c <= '9') {
            if (s_digits < 5) {
                s_val = s_val * 10 + (c - '0');
                s_digits++;
            }
        } else if (c == '-' && s_digits == 0) {
            s_neg = 1;
        } else if (c == '\n' || c == '\r') {
            uint8_t ok = s_digits;
            if (ok) {
                if (s_val > 32767) s_val = 32767;
                *value = (int16_t)(s_neg ? -s_val : s_val);
            }
            s_val = 0; s_neg = 0; s_digits = 0;
            if (ok) return 1;
        }
    }
    return 0;
}