#include "self_test.h"
#include "led_strip.h"
#include "audio.h"
#include "config.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "lvgl/lvgl.h"
#include <stdlib.h>

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

static void hstx_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // HSTX hardware handles automatic scanout - no explicit flush needed
    lv_display_flush_ready(disp);
}

void display_self_test(void)
{
    printf("[DISPLAY] Initializing HSTX DVI/HDMI display...\n");

    // Initialize LVGL
    lv_init();
    
    // Create LVGL display for HSTX (640x480)
    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    
    if (disp == NULL)
    {
        printf("[DISPLAY] ERROR: Failed to create LVGL display!\n");
        return;
    }

    // Allocate frame buffer for HSTX
    size_t frame_buffer_size = LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t);
    uint8_t *frame_buffer = malloc(frame_buffer_size);
    
    if (frame_buffer == NULL)
    {
        printf("[DISPLAY] ERROR: Failed to allocate frame buffer!\n");
        return;
    }

    // Set draw buffer
    lv_display_set_buffers(disp, frame_buffer, NULL, frame_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Set flush callback (minimal for HSTX - hardware handles scanout)
    lv_display_set_flush_cb(disp, hstx_flush_cb);

    printf("[DISPLAY] HSTX Display Ready (640x480)\n");
    printf("[DISPLAY] Frame buffer: %zu bytes\n", frame_buffer_size);
}