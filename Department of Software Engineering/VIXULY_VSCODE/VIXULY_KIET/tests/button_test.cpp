/* Test the actual button.cpp and motor.c against simulated AVR registers.
 * No electrical/timing/interrupt concurrency simulation is claimed.
 * Include button.cpp here to seed its private clock near uint32_t rollover.
 */
#include <avr/io.h>
volatile uint8_t PORTC, PINC, DDRC, DIDR0, PORTD, DDRD;
volatile uint8_t PRR, TCCR1A, TCCR1B, TCCR1C, TIMSK1, TIFR1;
volatile uint16_t TCNT1, OCR1A;
volatile uint8_t TIMSK2, TIFR2, OCR2A, OCR2B, TCCR2A, TCCR2B;

extern "C" {
#include "../src/motor.c"
}
#include "../src/button.cpp"

#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C"
#endif

static unsigned checks;
#define CHECK(expr) do { ++checks; if (!(expr)) return __LINE__; } while (0)

static void sample(uint8_t pins, uint32_t ms)
{
    PINC = pins;
    button_update();
    while (ms--) {
        TIMER1_COMPA_vect();
        button_update();
    }
}

static void reset(uint8_t held = 0)
{
    PORTC = DDRC = 0xA1;
    DIDR0 = 0xFF;
    PORTD = DDRD = 0x80;
    PRR = 0xFF;
    motor_init();
    button_init();
    sample(held, 60);
}

static bool outputs(uint8_t expected)
{
    return running == expected &&
        (PORTC & LED_MASK) == (expected == BUTTON_RUNNING ? RUN_LED_MASK : STOP_LED_MASK) &&
        !!(PORTD & _BV(MOTOR_SLP_BIT)) == (expected == BUTTON_RUNNING);
}

static void tap_run(void) { sample(RUN_MASK, 40); sample(0, 40); }
static void hold_run(void) { sample(RUN_MASK, BUTTON_LONG_PRESS_MS + 30); sample(0, 40); }

EXPORT int button_test_checks(void) { return (int)checks; }

EXPORT int run_button_tests(void)
{
    checks = 0;
    reset();
    CHECK(outputs(BUTTON_STOPPED));
    CHECK(button_get_mode() == BUTTON_MODE_POT && !button_is_selecting());
    CHECK((DDRC & INPUT_MASK) == 0 && (DDRC & LED_MASK) == LED_MASK);
    CHECK((PORTC & INPUT_MASK) == 0);
    CHECK((PORTC & 0xA1) == 0xA1 && (DDRC & 0xA1) == 0xA1);
    CHECK(DIDR0 == (uint8_t)(0xFF & ~(_BV(ADC1D) | _BV(ADC3D))));
    CHECK((PORTD & 0x80) && (DDRD & 0x80));
    CHECK(OCR1A == F_CPU / 64000UL - 1 && TCCR1A == 0 && TIMSK1 == _BV(OCIE1A));
    CHECK(TCCR1B == (_BV(WGM12) | _BV(CS11) | _BV(CS10)));
    CHECK(TCCR2A == (_BV(WGM21) | _BV(WGM20)) && TCCR2B == _BV(CS21));

    // Debounce both edges: noise alone cannot start; no repeated short press.
    sample(RUN_MASK, 7); sample(0, 7); sample(RUN_MASK, 8); sample(0, 30);
    CHECK(outputs(BUTTON_STOPPED));
    sample(RUN_MASK, 40);
    CHECK(outputs(BUTTON_STOPPED));
    sample(0, 5); sample(RUN_MASK, 5); sample(0, 19);
    CHECK(outputs(BUTTON_STOPPED));
    sample(0, 1);
    CHECK(outputs(BUTTON_RUNNING));
    tap_run();
    CHECK(outputs(BUTTON_RUNNING));

    // STOP takes effect on the debounced press, while still held.
    motor_set(123);
    CHECK(motor_get() == 123 && (TIMSK2 & _BV(OCIE2B)));
    sample(STOP_MASK, 10); sample(0, 30);
    CHECK(outputs(BUTTON_RUNNING));
    sample(STOP_MASK, 19);
    CHECK(outputs(BUTTON_RUNNING));
    sample(STOP_MASK, 1);
    CHECK(outputs(BUTTON_STOPPED) && motor_get() == 0);
    CHECK((TIMSK2 & (_BV(OCIE2A) | _BV(OCIE2B))) == 0);
    CHECK((PORTD & (_BV(MOTOR_IN1_BIT) | _BV(MOTOR_IN2_BIT))) == 0);
    sample(STOP_MASK | RUN_MASK, 2500); sample(RUN_MASK, 100); sample(0, 60);
    CHECK(outputs(BUTTON_STOPPED) && !button_is_selecting());
    tap_run();
    CHECK(outputs(BUTTON_RUNNING)); // s_awake and actual SLEEP agree after restart.

    // Menu entry and confirmation each require a separate hold.
    sample(RUN_MASK, 2019);
    CHECK(!button_is_selecting());
    sample(RUN_MASK, 1);
    CHECK(button_is_selecting());
    CHECK(outputs(BUTTON_STOP_ON_SELECT ? BUTTON_STOPPED : BUTTON_RUNNING));
    sample(RUN_MASK, 70000);
    CHECK(button_is_selecting() && button_get_selected_mode() == BUTTON_MODE_POT);
    sample(0, 40);
    CHECK(button_is_selecting() && button_get_selected_mode() == BUTTON_MODE_POT);
    tap_run();
    CHECK(button_get_selected_mode() == BUTTON_MODE_UART);
    CHECK(button_get_mode() == BUTTON_MODE_POT);
    tap_run();
    CHECK(button_get_selected_mode() == BUTTON_MODE_POT);
    tap_run(); hold_run();
    CHECK(!button_is_selecting() && button_get_mode() == BUTTON_MODE_UART);
    CHECK(outputs(BUTTON_STOP_ON_SELECT ? BUTTON_STOPPED : BUTTON_RUNNING));
    tap_run();
    CHECK(outputs(BUTTON_RUNNING));

    // STOP cancels an unconfirmed selection without changing committed mode.
    hold_run(); tap_run();
    CHECK(button_is_selecting() && button_get_selected_mode() == BUTTON_MODE_POT);
    sample(STOP_MASK, 20);
    CHECK(outputs(BUTTON_STOPPED) && !button_is_selecting());
    CHECK(button_get_mode() == BUTTON_MODE_UART && button_get_selected_mode() == BUTTON_MODE_UART);
    sample(0, 60);

    // Long press from STOP must not start the motor on press or release.
    hold_run();
    CHECK(button_is_selecting() && outputs(BUTTON_STOPPED));
    hold_run();
    CHECK(!button_is_selecting() && outputs(BUTTON_STOPPED));

    // Startup held RUN and simultaneous presses cannot start the motor.
    reset(RUN_MASK); sample(RUN_MASK, 3000); sample(0, 60);
    CHECK(outputs(BUTTON_STOPPED) && !button_is_selecting());
    sample(RUN_MASK | STOP_MASK, 100); sample(0, 60);
    CHECK(outputs(BUTTON_STOPPED));
    tap_run();
    CHECK(outputs(BUTTON_RUNNING));

    // STOP beginning exactly on RUN release suppresses that RUN gesture.
    reset(); sample(RUN_MASK, 40); sample(STOP_MASK, 40); sample(0, 60);
    CHECK(outputs(BUTTON_STOPPED));

    // Releasing just before the hold threshold must remain a short press.
    reset(); sample(RUN_MASK, 2019); sample(0, 40);
    CHECK(!button_is_selecting() && outputs(BUTTON_RUNNING));

    // uint32_t clock rollover during debounce and during long press.
    reset(); s_ms = UINT32_MAX - 10U;
    tap_run();
    CHECK(outputs(BUTTON_RUNNING));
    button_stop(); sample(0, 60);
    s_ms = UINT32_MAX - 1000U;
    hold_run();
    CHECK(button_is_selecting() && outputs(BUTTON_STOPPED));
    button_stop();
    CHECK(!button_is_selecting() && outputs(BUTTON_STOPPED));
    return 0;
}
