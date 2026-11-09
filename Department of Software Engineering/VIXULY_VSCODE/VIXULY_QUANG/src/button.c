#include <Arduino.h>      /* millis() */
#include <avr/io.h>
#include "button.h"
#include "config.h"

static inline uint8_t read_pressed(uint8_t mask)
{
    uint8_t high = (PINC & mask) ? 1 : 0;
#if BTN_ACTIVE_HIGH
    return high;
#else
    return !high;
#endif
}

void btn_init(button_t *b, uint8_t pc_bit)
{
    b->mask = _BV(pc_bit);
    DDRC &= ~b->mask;                 /* input                                */
#if BTN_ACTIVE_HIGH
    PORTC &= ~b->mask;                /* external pull-down on the board      */
#else
    PORTC |= b->mask;                 /* internal pull-up for active-low      */
#endif
    b->raw       = read_pressed(b->mask);
    b->stable    = b->raw;
    b->long_sent = 0;
    b->t_change  = millis();
    b->t_press   = b->t_change;
}

uint8_t btn_is_down(const button_t *b) { return b->stable; }

btn_event_t btn_update(button_t *b)
{
    uint32_t now = millis();
    uint8_t  raw = read_pressed(b->mask);

    if (raw != b->raw) {                          /* bouncing: restart timer */
        b->raw      = raw;
        b->t_change = now;
    } else if (raw != b->stable &&
               (uint32_t)(now - b->t_change) >= BTN_DEBOUNCE_MS) {
        b->stable = raw;                          /* accepted a new level    */
        if (raw) {                                /* press                   */
            b->t_press   = now;
            b->long_sent = 0;
        } else if (!b->long_sent) {               /* release after short hold */
            return BTN_EVT_SHORT;
        }
    }

    if (b->stable && !b->long_sent &&
        (uint32_t)(now - b->t_press) >= BTN_LONG_MS) {
        b->long_sent = 1;
        return BTN_EVT_LONG;
    }
    return BTN_EVT_NONE;
}