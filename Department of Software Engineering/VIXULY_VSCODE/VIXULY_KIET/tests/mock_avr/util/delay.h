#ifndef MOCK_AVR_DELAY_H
#define MOCK_AVR_DELAY_H
static inline void _delay_us(unsigned long us) { (void)us; }
#endif
