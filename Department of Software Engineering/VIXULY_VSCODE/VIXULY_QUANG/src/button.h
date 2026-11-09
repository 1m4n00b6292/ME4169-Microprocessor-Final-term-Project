/*
 * button.h - debounced push button with short / long press detection
 *            (bare-metal, PORTC, polled; uses millis())
 *
 *   BTN_EVT_SHORT : fired on RELEASE, if the press was shorter than BTN_LONG_MS
 *   BTN_EVT_LONG  : fired ONCE while still holding, when BTN_LONG_MS is reached
 *                   (the following release produces no event)
 *
 * Call btn_update() often (every loop). It returns at most one event per call.
 * Tunables (pins, polarity, debounce, long-press time) live in config.h.
 */
#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_SHORT,
    BTN_EVT_LONG
} btn_event_t;

typedef struct {
    uint8_t  mask;        /* _BV(bit) on PORTC                   */
    uint8_t  raw;         /* last raw reading (1 = pressed)      */
    uint8_t  stable;      /* debounced state  (1 = pressed)      */
    uint8_t  long_sent;   /* long event already delivered        */
    uint32_t t_change;    /* time of last raw change             */
    uint32_t t_press;     /* time the debounced press started    */
} button_t;

void        btn_init(button_t *b, uint8_t pc_bit);   /* sets PCx as input, no pull-up */
btn_event_t btn_update(button_t *b);
uint8_t     btn_is_down(const button_t *b);          /* debounced level */

#ifdef __cplusplus
}
#endif
#endif