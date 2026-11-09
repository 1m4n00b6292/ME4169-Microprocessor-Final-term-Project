/* C entry point also verifies the C linkage of button.cpp and motor.c. */
#include <avr/interrupt.h>
#include "button.h"
#include "motor.h"

int main(void)
{
    motor_init();
    button_init();
    sei();
    for (;;) button_update();
}
