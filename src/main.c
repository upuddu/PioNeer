#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"
#include "gpio_buttons.h"
#include "led_strip.h"

int main(void) {
    stdio_init_all();

    // Init buttons on GPIO34-37
    buttons_init();

    // Enable interrupts — prints button name on press
    buttons_enable_interrupts();

    // Init WS2812B LED strip on GPIO9 (20 LEDs)
    led_strip_init();

    printf("System started. Buttons + LED strip ready.\n");

    // Visible boot self-test loop:
    // - solid colors
    // - per-pixel RGB pattern
    // - brightness ramp
    // - rainbow animation
    while (true) {
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

        // Per-pixel repeating pattern
        led_strip_clear();
        for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
            if ((i % 3) == 0) {
                led_strip_set_pixel(i, 255, 0, 0);
            } else if ((i % 3) == 1) {
                led_strip_set_pixel(i, 0, 255, 0);
            } else {
                led_strip_set_pixel(i, 0, 0, 255);
            }
        }
        led_strip_show();
        sleep_ms(800);

        // Global brightness ramp down/up
        for (int b = 255; b >= 0; b -= 10) {
            led_strip_set_brightness((uint8_t)b);
            led_strip_show();
            sleep_ms(20);
        }
        for (int b = 0; b <= 255; b += 10) {
            led_strip_set_brightness((uint8_t)b);
            led_strip_show();
            sleep_ms(20);
        }

        // Rainbow animation (one full cycle)
        led_strip_set_brightness(128);
        led_strip_rainbow_cycle(10);

        // Rainbow loading animation (progressive fill)
        led_strip_rainbow_loading(100);

        // Demo new functions
        led_strip_clear();
        led_strip_set_pixel(0, 255, 0, 0);  // Red on first LED
        led_strip_set_pixel(WS2812_NUM_LEDS-1, 0, 255, 0);  // Green on last LED
        led_strip_show();
        sleep_ms(1000);

        led_strip_wave(COLOR_BLUE, 3, 50);  // Blue wave effect
        led_strip_bounce(COLOR_YELLOW, 100);  // Yellow bouncing light
        led_strip_pulse_pixel(2, COLOR_MAGENTA, 500);  // Pulse LED 2 magenta
        led_strip_blink_pixel(5, COLOR_CYAN, 200, 200, 3);  // Blink LED 5 cyan 3 times
        led_strip_random_fill();  // Random colors
        led_strip_show();
        sleep_ms(2000);
    }

    return 0;
}
