#ifndef MOCK_AVR_ATOMIC_H
#define MOCK_AVR_ATOMIC_H
/* Host tests are single-threaded; real AVR build uses avr-libc atomic.h. */
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(type) for (uint8_t mock_once = 1; mock_once; mock_once = 0)
#endif
