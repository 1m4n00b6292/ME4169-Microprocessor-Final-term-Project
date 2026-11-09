/*
 * encoder_mini - quadrature encoder (x4) for ATmega88PA, VI XU LY board
 *
 * ENC_A -> PD2 (INT0, Arduino pin 2)
 * ENC_B -> PD3 (INT1, Arduino pin 3)
 *
 * Both channels use "any logic change" external interrupts, so every edge
 * is decoded (4 counts per encoder cycle). Only ONE instance is possible
 * because the ISRs are fixed to INT0/INT1: use the global object `enc`.
 *
 * Swap direction: define ENC_INVERT before including, or swap A/B wires.
 */
#pragma once
#include <Arduino.h>

class EncoderMini {
public:
    // pullups = true enables internal pull-ups on PD2/PD3
    void begin(bool pullups = true);

    // Position in x4 counts (atomic read)
    int32_t count();
    void    reset(int32_t value = 0);

    // Counts per revolution at x4 (= PPR * 4 for a standard A/B encoder)
    void    setCountsPerRev(uint16_t cpr) { _cpr = cpr; }

    // Call often from loop(). Every `window_ms` it refreshes the velocity.
    // Returns true when a new velocity value has just been computed.
    bool    update(uint16_t window_ms = 50);

    int32_t velocity() const { return _vel; }          // counts / second (signed)
    int32_t rpm() const;                                // needs setCountsPerRev()

private:
    int32_t  _vel  = 0;
    int32_t  _last = 0;
    uint32_t _t    = 0;
    uint16_t _cpr  = 0;
};

extern EncoderMini enc;