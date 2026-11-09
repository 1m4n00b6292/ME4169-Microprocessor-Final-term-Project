/*
 * uart.h - minimal USART0 receiver (bare-metal, interrupt + ring buffer)
 *
 * RX only (PD0). TX is left disabled so PD1 stays a normal pin.
 * Protocol: ASCII lines, one signed integer per line, ended by '\n' or '\r'
 *     "300\n"   -> 300
 *     "-120\r"  -> -120
 * Other characters are ignored. Baud / buffer size are set in config.h.
 */
#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void    uart_init(void);
void    uart_flush(void);                 /* drop buffered bytes + partial line */
uint8_t uart_poll_int(int16_t *value);    /* 1 when a full line was parsed      */

#ifdef __cplusplus
}
#endif
#endif