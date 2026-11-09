#include "encoder_mini.h"
#include "config.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>

EncoderMini enc;

static volatile int32_t s_count = 0;
static volatile uint8_t s_prev  = 0;

// index = (old << 2) | new, state = (A << 1) | B
// Gray sequence 00 -> 01 -> 11 -> 10 counts +1; invalid jumps count 0
#ifdef ENC_INVERT
static const int8_t kTable[16] PROGMEM = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
#else
static const int8_t kTable[16] PROGMEM = {0,1,-1,0, -1,0,0,1, 1,0,0,-1, 0,-1,1,0};
#endif

#if ENC_FILTER_N > 1
/* ======================================================================
 * Filtered mode (like STM32 ICxF): Timer1 samples the pins at
 * ENC_FILTER_SAMPLE_HZ. A channel only changes level after ENC_FILTER_N
 * consecutive samples show the new level; shorter pulses are ignored.
 * ====================================================================== */
#define ENC_T1_TOP  ((uint16_t)((F_CPU / ENC_FILTER_SAMPLE_HZ) - 1))

static uint8_t s_fa = 0, s_fb = 0;        /* filtered levels  */
static uint8_t s_na = 0, s_nb = 0;        /* agreeing samples */

ISR(TIMER1_COMPA_vect)
{
    uint8_t p       = PIND;
    uint8_t changed = 0;

    uint8_t a = (p & _BV(ENC_A_BIT)) ? 1 : 0;
    if (a != s_fa) {
        if (++s_na >= ENC_FILTER_N) { s_fa = a; s_na = 0; changed = 1; }
    } else {
        s_na = 0;
    }

    uint8_t b = (p & _BV(ENC_B_BIT)) ? 1 : 0;
    if (b != s_fb) {
        if (++s_nb >= ENC_FILTER_N) { s_fb = b; s_nb = 0; changed = 1; }
    } else {
        s_nb = 0;
    }

    if (changed) {
        uint8_t cur = (uint8_t)((s_fa << 1) | s_fb);
        s_count += (int8_t)pgm_read_byte(&kTable[(s_prev << 2) | cur]);
        s_prev = cur;
    }
}
#else
/* ======================================================================
 * Unfiltered mode: any-change interrupts on INT0 / INT1
 * ====================================================================== */
static inline void encStep()
{
    uint8_t p   = PIND;
    uint8_t cur = ((p & _BV(ENC_A_BIT)) ? 2 : 0) | ((p & _BV(ENC_B_BIT)) ? 1 : 0);
    s_count += (int8_t)pgm_read_byte(&kTable[(s_prev << 2) | cur]);
    s_prev = cur;
}

ISR(INT0_vect) { encStep(); }
ISR(INT1_vect) { encStep(); }
#endif

void EncoderMini::begin(bool pullups)
{
    DDRD &= ~(_BV(ENC_A_BIT) | _BV(ENC_B_BIT));          // inputs
    if (pullups) PORTD |=  (_BV(ENC_A_BIT) | _BV(ENC_B_BIT));
    else         PORTD &= ~(_BV(ENC_A_BIT) | _BV(ENC_B_BIT));

    uint8_t p = PIND;
    uint8_t a = (p & _BV(ENC_A_BIT)) ? 1 : 0;
    uint8_t b = (p & _BV(ENC_B_BIT)) ? 1 : 0;
    s_prev  = (a << 1) | b;
    s_count = 0;

#if ENC_FILTER_N > 1
    s_fa = a; s_fb = b;
    s_na = 0; s_nb = 0;

    TCCR1A = 0;                                           // CTC, TOP = OCR1A
    TCCR1B = _BV(WGM12) | _BV(CS10);                      // prescaler 1
    OCR1A  = ENC_T1_TOP;
    TCNT1  = 0;
    TIFR1  = _BV(OCF1A);
    TIMSK1 |= _BV(OCIE1A);
#else
    EICRA = _BV(ISC00) | _BV(ISC10);                      // any change on INT0 and INT1
    EIFR  = _BV(INTF0) | _BV(INTF1);                      // clear stale flags
    EIMSK |= _BV(INT0) | _BV(INT1);
#endif

    _last = 0;
    _t    = millis();
}

int32_t EncoderMini::count()
{
    int32_t c;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { c = s_count; }
    return c;
}

void EncoderMini::reset(int32_t value)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { s_count = value; }
    _last = value;
    _t    = millis();
    _vel  = 0;
}

bool EncoderMini::update(uint16_t window_ms)
{
    uint32_t now = millis();
    uint32_t dt  = now - _t;
    if (dt < window_ms) return false;

    int32_t c = count();
    _vel  = ((c - _last) * 1000L) / (int32_t)dt;   // 32-bit is enough (|delta| < 2M per window)
    _last = c;
    _t    = now;
    return true;
}

int32_t EncoderMini::rpm() const
{
    if (_cpr == 0) return 0;
    return (_vel * 60L) / _cpr;
}