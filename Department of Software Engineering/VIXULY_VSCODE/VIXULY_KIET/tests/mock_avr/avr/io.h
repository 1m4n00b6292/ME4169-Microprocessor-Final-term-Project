#ifndef MOCK_AVR_IO_H
#define MOCK_AVR_IO_H
#include <stdint.h>
#define _BV(bit) (1U << (bit))
enum {
    PC1=1, PC2=2, PC3=3, PC4=4, ADC1D=1, ADC3D=3,
    PRTIM1=3, ICF1=5, OCF1B=2, OCF1A=1, TOV1=0, OCIE1A=1,
    WGM12=3, CS11=1, CS10=0,
    CS21=1, CS20=0, CS22=2, OCIE2A=1, OCIE2B=2,
    OCF2A=1, OCF2B=2, WGM21=1, WGM20=0, TOV2=0, TOIE2=0,
    FE0=4, DOR0=3, UPE0=2, PRUSART0=1, UCSZ01=2, UCSZ00=1,
    RXEN0=4, RXCIE0=7
};
extern volatile uint8_t PORTC, PINC, DDRC, DIDR0, PORTD, DDRD;
extern volatile uint8_t PRR, TCCR1A, TCCR1B, TCCR1C, TIMSK1, TIFR1;
extern volatile uint16_t TCNT1, OCR1A;
extern volatile uint8_t TIMSK2, TIFR2, OCR2A, OCR2B, TCCR2A, TCCR2B;
extern volatile uint8_t UCSR0A, UCSR0B, UCSR0C, UDR0, UBRR0H, UBRR0L;
#endif
