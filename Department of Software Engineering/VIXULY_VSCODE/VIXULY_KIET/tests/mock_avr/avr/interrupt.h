#ifndef MOCK_AVR_INTERRUPT_H
#define MOCK_AVR_INTERRUPT_H
#include "io.h"
#define ISR(vector) void vector(void)
#endif
