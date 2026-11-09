#include "adc.h"
#include <avr/io.h>

void adc_init(void)
{
    // AVCC as reference, ADC0 initially selected
    ADMUX = _BV(REFS0);

    // Enable ADC
    // Prescaler = 128
    // 16 MHz / 128 = 125 kHz ADC clock
    ADCSRA = _BV(ADEN) |
             _BV(ADPS2) |
             _BV(ADPS1) |
             _BV(ADPS0);
}

uint16_t adc_read(uint8_t channel)
{
    // Select ADC channel 0-7
    ADMUX = _BV(REFS0) | (channel & 0x07);

    // Start conversion
    ADCSRA |= _BV(ADSC);

    // Wait for conversion to finish
    while (ADCSRA & _BV(ADSC))
        ;

    return ADC;
}