/* Reuse actual motor/button code and button gestures from the unit tests. */
#include "button_test.cpp"
#include "../src/main.cpp"
#include "../src/motion.h"

volatile uint8_t UCSR0A, UCSR0B, UCSR0C, UDR0, UBRR0H, UBRR0L;
static uint16_t mockAdc;
static uint8_t displayedRunning, displayBacklight;

extern "C" unsigned long millis(void) { return s_ms; }
extern "C" void adc_init(void) {}
extern "C" uint16_t adc_read(uint8_t) { return mockAdc; }
extern "C" void tft_init(void) {}
extern "C" void tft_backlight(uint8_t on) { displayBacklight = on; }
extern "C" void tft_fill_rect(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t) {}
extern "C" uint8_t tft_text(uint8_t x, uint8_t, const char *, uint16_t, uint16_t, uint8_t) { return x; }
extern "C" uint8_t tft_text_P(uint8_t x, uint8_t y, const char *s, uint16_t, uint16_t, uint8_t)
{
    if (y == 80) displayedRunning = s[0] == 'R';
    return x;
}
extern "C" uint8_t tft_int(uint8_t x, uint8_t, int16_t, uint8_t, uint16_t, uint16_t, uint8_t) { return x; }

static void receive(const char *s)
{
    while (*s) {
        UCSR0A = 0;
        UDR0 = (uint8_t)*s++;
        USART_RX_vect();
    }
}

EXPORT int run_app_tests(void)
{
    checks = 0;
    PINC = 0;
    setup();
    sample(0, 60);
    CHECK(outputs(BUTTON_STOPPED) && displayBacklight == 1 && displayedRunning == 0);
    CHECK(UBRR0H == 0 && UBRR0L == 103);
    CHECK(UCSR0C == (_BV(UCSZ01) | _BV(UCSZ00)));
    CHECK(UCSR0B == (_BV(RXEN0) | _BV(RXCIE0)));

    // POT maps ADC 0..1023 to PWM 0..255, and UART cannot change POT output.
    mockAdc = 512;
    tap_run(); loop();
    CHECK(outputs(BUTTON_RUNNING) && motor_get() == 128);
    receive("200\r\n"); loop();
    CHECK(uartDuty == 200 && motor_get() == 128);
    mockAdc = 1023; loop();
    CHECK(motor_get() == 255);
    mockAdc = 0; loop();
    CHECK(motor_get() == 0);
    showState();
    CHECK(displayedRunning == 1);

    // STOP stays asleep across repeated loops, incoming UART and high POT.
    mockAdc = 1023;
    sample(STOP_MASK, 20); loop();
    CHECK(outputs(BUTTON_STOPPED) && motor_get() == 0);
    receive("255\n");
    for (uint8_t i = 0; i < 50; ++i) loop();
    CHECK(outputs(BUTTON_STOPPED) && motor_get() == 0);
    showState();
    CHECK(displayedRunning == 0);
    sample(0, 60);

    // Commit UART mode; no auto-run on menu confirmation.
    hold_run(); tap_run(); hold_run(); loop();
    CHECK(button_get_mode() == BUTTON_MODE_UART && outputs(BUTTON_STOPPED));
    tap_run(); loop();
    CHECK(outputs(BUTTON_RUNNING) && motor_get() == 255);
    receive("127"); loop();
    CHECK(motor_get() == 255); // incomplete line cannot alter the command
    receive("\r\n"); loop();
    CHECK(motor_get() == 127);
    mockAdc = 0; loop();
    CHECK(motor_get() == 127);
    receive("256\n-1\n12x\n999999999999\n\n\r\n 1\n"); loop();
    CHECK(motor_get() == 127);
    receive("0\n"); loop();
    CHECK(motor_get() == 0 && outputs(BUTTON_RUNNING));
    receive("255\n"); loop();
    CHECK(motor_get() == 255);

    // Framing/parity/overrun errors poison a complete line, then recover.
    const uint8_t errors[] = {_BV(FE0), _BV(DOR0), _BV(UPE0)};
    for (uint8_t i = 0; i < 3; ++i) {
        receive("1");
        UCSR0A = errors[i]; UDR0 = '2'; USART_RX_vect();
        receive("3\n"); loop();
        CHECK(motor_get() == 255);
    }
    receive("42\n"); loop();
    CHECK(motor_get() == 42);

    // Entering menu from RUN follows configured behavior; STOP still wins.
    hold_run(); loop();
    CHECK(button_is_selecting());
    CHECK(outputs(BUTTON_STOP_ON_SELECT ? BUTTON_STOPPED : BUTTON_RUNNING));
    sample(STOP_MASK, 20); loop();
    CHECK(outputs(BUTTON_STOPPED) && !button_is_selecting());

    // Motion's C API can be called from C++; STOP must not wake SLEEP.
    motion_init();
    CHECK(!(PORTD & _BV(MOTOR_SLP_BIT)) && motion_is_running() == 0);
    motion_set_feedforward(256); // Q8=1, no arbitrary motor PID tuning
    motion_set_target(100);
    motion_start();
    s_ms += 20;
    CHECK(motion_update(0) == 1 && motion_get_output() == 100);
    CHECK((PORTD & _BV(MOTOR_SLP_BIT)) && motor_get() == 100);
    button_stop();
    motion_stop();
    CHECK(outputs(BUTTON_STOPPED) && motor_get() == 0 && motion_is_running() == 0);
    s_ms += 100;
    CHECK(motion_update(0) == 0 && outputs(BUTTON_STOPPED));
    motion_start(); s_ms += 20;
    CHECK(motion_update(0) == 1 && motor_get() == 100);
    motion_stop();
    CHECK(!(PORTD & _BV(MOTOR_SLP_BIT)) && motor_get() == 0);
    return 0;
}
