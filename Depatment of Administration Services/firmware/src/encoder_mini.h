/*
 * encoder_mini - quadrature encoder (x4) for ATmega88PA, VI XU LY board
 *
 * ENC_A -> PD2 (INT0, Arduino pin 2)
 * ENC_B -> PD3 (INT1, Arduino pin 3)
 *
 * Every edge is decoded (4 counts per encoder cycle). With ENC_FILTER_N > 1
 * (config.h) both channels pass an STM32-style digital input filter sampled by
 * Timer1; with ENC_FILTER_N <= 1 the "any logic change" INT0/INT1 interrupts
 * are used instead. Only ONE instance is possible because the ISRs are fixed:
 * use the global object `enc`.
 *
 * Swap direction: define ENC_INVERT before including, or swap A/B wires.
 */
#pragma once
#include <Arduino.h>
#include "config.h"

class EncoderMini {
public:
    // pullups = true enables internal pull-ups on PD2/PD3
    void begin(bool pullups = true);

    // Position in x4 counts (atomic read)
    int32_t count();
    void    reset(int32_t value = 0);

    // Counts per revolution at x4 (= PPR * 4 for a standard A/B encoder)
    void    setCountsPerRev(uint16_t cpr) { _cpr = ENC_CPR; }

    // Call often from loop(). Every `window_ms` it refreshes the velocity.
    // Returns true when a new velocity value has just been computed.
    bool    update(uint16_t window_ms = ENC_WINDOW_MS);

    int32_t velocity() const { return _vel; }          // counts / second (signed)
    int32_t rpm() const;                                // needs setCountsPerRev()

private:
    int32_t  _vel  = 0;
    int32_t  _last = 0;
    uint32_t _t    = 0;
    uint16_t _cpr  = 0;
};

extern EncoderMini enc;