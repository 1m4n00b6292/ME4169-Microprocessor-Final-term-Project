/*
 * app.h - application layer for the VI XU LY board
 *
 * Ties together: TFT, ADC (pot), UART, encoder, motor, motion (PID), buttons,
 * LEDs, and implements the logic in VIXULY_Logic.txt:
 *
 *   STOP button  short press        : stop (motor SLEEP pin PD4 low)
 *   RUN  button  short press        : STOP -> RUN
 *   RUN  button  long press (~2 s)  : (from STOP) enter mode selection
 *     in selection: RUN short = swap POTENTIOMETER / UART, RUN long = confirm,
 *                   STOP = cancel
 *   RUN LED  (PC2) on only while running
 *   STOP LED (PC4) on only while stopped / selecting
 *
 * main.cpp only needs app_init() in setup() and app_update() in loop().
 */
#ifndef APP_H
#define APP_H

#include <stdint.h>

/* 1 = system running, 0 = stopped (also 0 while in mode selection) */
extern uint8_t running;

void    app_init(void);
void    app_update(void);

#endif