#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#ifdef __cplusplus
extern "C" {
#endif
unsigned long millis(void);
#ifdef __cplusplus
}
#endif
#endif
