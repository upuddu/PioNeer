#include "self_test.h"
#include "led_strip.h"
#include "audio.h"
#include "config.h"
#include <stdio.h>
#include "pico/stdlib.h"

void led_self_test(void)
{
    printf("[LED] Starting self-test...\n");

    led_strip_set_all_color(COLOR_RED);
    led_strip_show();
    sleep_ms(400);
    led_strip_set_all_color(COLOR_GREEN);
    led_strip_show();
    sleep_ms(400);
    led_strip_set_all_color(COLOR_BLUE);
    led_strip_show();
    sleep_ms(400);

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

    led_strip_set_brightness(128);
    led_strip_rainbow_cycle(10);
    led_strip_rainbow_loading(100);

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

void audio_self_test(void)
{
    printf("[AUDIO] Starting self-test...\n");

    printf("[AUDIO] 1. SINE - C Major Scale\n");
    set_waveform(WAVE_SINE);
    play_melody();
    sleep_ms(500);

    printf("[AUDIO] 2. SAWTOOTH - Frequency Sweep\n");
    set_waveform(WAVE_SAWTOOTH);
    play_sweep();
    sleep_ms(500);

    printf("[AUDIO] 3. SQUARE - C Major Scale\n");
    set_waveform(WAVE_SQUARE);
    play_melody();
    sleep_ms(500);

    printf("[AUDIO] 4. TRIANGLE - C Major Scale\n");
    set_waveform(WAVE_TRIANGLE);
    play_melody();
    sleep_ms(500);

    printf("[AUDIO] 5. SINE - C Major Chord\n");
    set_waveform(WAVE_SINE);
    play_chord();
    sleep_ms(500);

    printf("[AUDIO] 6. SAWTOOTH - C Major Chord\n");
    set_waveform(WAVE_SAWTOOTH);
    play_chord();

    set_freq(0, 0);
    set_freq(1, 0);
    printf("[AUDIO] Self-test complete.\n");
}