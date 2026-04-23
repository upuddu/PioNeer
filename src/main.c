#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "config.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "led_strip.h"
#include "math.h"

#ifndef M_PI
#define M_PI 3.141592f
#endif

// ── Button callback ───────────────────────────────────────────────────────────
static const char *button_names[] = {"BTN_A", "BTN_B", "BTN_X", "BTN_Y"};

void button_event_handler(Button btn, ButtonState state)
{
    const char *state_str = (state == BTN_STATE_PRESSED) ? "pressed" : "released";
    printf("[BUTTONS] %s %s\n", button_names[btn], state_str);
}

// ── LED self-test ─────────────────────────────────────────────────────────────
static void led_self_test(void)
{
    printf("[LED] Starting self-test...\n");

    // Solid colors
    led_strip_set_all_color(COLOR_RED);
    led_strip_show();
    sleep_ms(400);
    led_strip_set_all_color(COLOR_GREEN);
    led_strip_show();
    sleep_ms(400);
    led_strip_set_all_color(COLOR_BLUE);
    led_strip_show();
    sleep_ms(400);

    // Per-pixel RGB pattern
    led_strip_clear();
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++)
    {
        if ((i % 3) == 0)
            led_strip_set_pixel(i, 255, 0, 0);
        else if ((i % 3) == 1)
            led_strip_set_pixel(i, 0, 255, 0);
        else
            led_strip_set_pixel(i, 0, 0, 255);
    }
    led_strip_show();
    sleep_ms(800);

    // Brightness ramp
    for (int b = 255; b >= 0; b -= 10)
    {
        led_strip_set_brightness((uint8_t)b);
        led_strip_show();
        sleep_ms(20);
    }
    for (int b = 0; b <= 255; b += 10)
    {
        led_strip_set_brightness((uint8_t)b);
        led_strip_show();
        sleep_ms(20);
    }

    // Rainbow + effects
    led_strip_set_brightness(128);
    led_strip_rainbow_cycle(10);
    led_strip_rainbow_loading(100);

    // Individual pixel effects
    led_strip_clear();
    led_strip_set_pixel(0, 255, 0, 0);
    led_strip_set_pixel(WS2812_NUM_LEDS - 1, 0, 255, 0);
    led_strip_show();
    sleep_ms(1000);

    led_strip_wave(COLOR_BLUE, 3, 50);
    led_strip_bounce(COLOR_YELLOW, 100);
    led_strip_pulse_pixel(2, COLOR_MAGENTA, 500);
    led_strip_blink_pixel(5, COLOR_CYAN, 200, 200, 3);
    led_strip_random_fill();
    led_strip_show();
    sleep_ms(2000);

    led_strip_clear();
    led_strip_show();
    printf("[LED] Self-test complete.\n");
}

// ── Joystick read + print ─────────────────────────────────────────────────────
static void joystick_poll(void)
{
    JoystickReading joy = joystick_read();
    int dx = (int)joy.x - 2048;
    int dy = (int)joy.y - 2048;

    if (dx > -100 && dx < 100 && dy > -100 && dy < 100)
    {
        printf("[JOY] CENTER\n");
    }
    else
    {
        float angle = atan2f((float)dy, (float)dx) * 180.0f / M_PI;
        if (angle < 0)
            angle += 360.0f;
        printf("[JOY] Angle: %.1f deg\n", angle);
    }

    if (joystick_sw_consume())
    {
        printf("[JOY] CLICK\n");
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(void)
{
    stdio_init_all();
    sleep_ms(2000);
    printf("=== BOOT ===\n");
    printf("=== PioNeer System Test ===\n");

    // Buttons FIRST — registers the global IO_IRQ_BANK0 handler
    printf("[INIT] Buttons...\n");
    buttons_init();
    buttons_set_callback(button_event_handler);

    // Joystick AFTER — can now safely enable its IRQ pin
    printf("[INIT] Joystick...\n");
    joystick_init();
    joystick_sw_init();

    printf("[INIT] LED strip...\n");
    led_strip_init();

    led_self_test();

    printf("[MAIN] Entering main loop.\n");
    while (true)
    {
        joystick_poll();
        sleep_ms(100);
    }
    return 0;
}