#include "reaction.h"
#include "gpio_buttons.h"
#include "adc_joystick.h"
#include "audio.h"
#include "led_strip.h"
#include "config.h"
#include "lvgl.h"
#include "pico/stdlib.h"
#include <stdlib.h>
#include <stdio.h>

#define NUM_ROUNDS 5

// Screen colors for each button
#define COLOR_BG lv_color_hex(0x111111)
#define COLOR_X lv_color_hex(0x2196F3) // blue
#define COLOR_A lv_color_hex(0x4CAF50) // green
#define COLOR_Y lv_color_hex(0xFFC107) // yellow
#define COLOR_B lv_color_hex(0xF44336) // red
#define COLOR_TEXT lv_color_hex(0xFFFFFF)
#define COLOR_WAIT lv_color_hex(0x424242)

static const char *btn_letters[] = {"A", "B", "X", "Y"};

// Screen color for each button
static lv_color_t screen_color_for(Button b)
{
    switch (b)
    {
    case BTN_X:
        return COLOR_X;
    case BTN_A:
        return COLOR_A;
    case BTN_Y:
        return COLOR_Y;
    case BTN_B:
        return COLOR_B;
    }
    return COLOR_BG;
}

// LED strip color for each button (r, g, b)
static void led_color_for(Button b, uint8_t *r, uint8_t *g, uint8_t *bl)
{
    switch (b)
    {
    case BTN_X:
        *r = 30;
        *g = 100;
        *bl = 255;
        break; // blue
    case BTN_A:
        *r = 50;
        *g = 255;
        *bl = 50;
        break; // green
    case BTN_Y:
        *r = 255;
        *g = 200;
        *bl = 0;
        break; // yellow
    case BTN_B:
        *r = 255;
        *g = 40;
        *bl = 40;
        break; // red
    default:
        *r = 0;
        *g = 0;
        *bl = 0;
        break;
    }
}

// ── Audio helpers ─────────────────────────────────────────────────────────────
static void play_tone(float freq, uint32_t ms)
{
    set_freq(0, freq);
    sleep_ms(ms);
    set_freq(0, 0);
}

static void play_ding(void)
{
    set_waveform(WAVE_SINE);
    play_tone(523, 80);  // C5
    play_tone(784, 120); // G5
}

static void play_error(void)
{
    set_waveform(WAVE_SQUARE);
    set_freq(0, 150);
    sleep_ms(400);
    set_freq(0, 0);
    set_waveform(WAVE_SINE);
}

static void play_ready_beep(void)
{
    set_waveform(WAVE_SINE);
    play_tone(330, 60);
}

static void play_target_beep(void)
{
    set_waveform(WAVE_SINE);
    play_tone(880, 80);
}

// ── LED helpers ───────────────────────────────────────────────────────────────
static void led_off(void)
{
    led_strip_clear();
    led_strip_show();
}

static void led_all(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++)
    {
        led_strip_set_pixel(i, r, g, b);
    }
    led_strip_show();
}

// Flash red for ~2 seconds
static void led_flash_red(void)
{
    for (int i = 0; i < 8; i++)
    {
        led_all(0, 210, 210); // full red
        sleep_ms(125);
        led_all(0, 0, 0); // dim red instead of off — stays red but flickers
        sleep_ms(125);
    }
    led_all(255, 40, 40); // end on bright red
    sleep_ms(200);
    led_off();
}

static void led_flash_green(void)
{
    for (int i = 0; i < 8; i++)
    {
        led_all(50, 255, 50); // full red
        sleep_ms(125);
        led_all(0, 0, 0); // dim red instead of off — stays red but flickers
        sleep_ms(125);
    }
    led_all(50, 255, 50); // end on bright red
    sleep_ms(200);
    led_off();
}

// ── Button polling ────────────────────────────────────────────────────────────
static int poll_pressed(void)
{
    static ButtonState prev[4] = {
        BTN_STATE_RELEASED, BTN_STATE_RELEASED,
        BTN_STATE_RELEASED, BTN_STATE_RELEASED};
    for (int i = 0; i < 4; i++)
    {
        ButtonState s = button_get_state((Button)i);
        if (s == BTN_STATE_PRESSED && prev[i] != BTN_STATE_PRESSED)
        {
            prev[i] = s;
            return i;
        }
        prev[i] = s;
    }
    return -1;
}

// ── Main game ─────────────────────────────────────────────────────────────────
void reaction_run(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    // Center letter/text
    lv_obj_t *letter = lv_label_create(scr);
    lv_obj_set_style_text_color(letter, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(letter, &lv_font_montserrat_32, 0);
    lv_label_set_text(letter, "Ready?");
    lv_obj_center(letter);

    // Top status
    lv_obj_t *status = lv_label_create(scr);
    lv_obj_set_style_text_color(status, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_16, 0);
    lv_label_set_text(status, "Reaction Time");
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 8);

    // Round counter
    lv_obj_t *round_label = lv_label_create(scr);
    lv_obj_set_style_text_color(round_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(round_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(round_label, "");
    lv_obj_align(round_label, LV_ALIGN_TOP_LEFT, 8, 8);

    led_off();
    lv_timer_handler();
    sleep_ms(1500);

    uint32_t times[NUM_ROUNDS];
    int valid = 0;

    for (int round = 0; round < NUM_ROUNDS; round++)
    {
        char rbuf[16];
        snprintf(rbuf, sizeof(rbuf), "Round %d/%d", round + 1, NUM_ROUNDS);
        lv_label_set_text(round_label, rbuf);

        // ── Wait phase ──────────────────────────────────────────────────────
        lv_obj_set_style_bg_color(scr, COLOR_WAIT, 0);
        lv_label_set_text(letter, "...");
        lv_obj_center(letter);
        led_off();
        lv_timer_handler();
        play_ready_beep();

        uint32_t wait_ms = 1500 + (rand() % 2500);
        uint32_t start_wait = to_ms_since_boot(get_absolute_time());
        bool fouled = false;

        while (to_ms_since_boot(get_absolute_time()) - start_wait < wait_ms)
        {
            if (poll_pressed() != -1)
            {
                fouled = true;
                break;
            }
            lv_timer_handler();
            sleep_ms(5);
        }

        if (fouled)
        {
            lv_obj_set_style_bg_color(scr, lv_color_hex(0x8B0000), 0);
            lv_label_set_text(letter, "TOO EARLY!");
            lv_obj_center(letter);
            lv_timer_handler();
            play_error();
            led_flash_red();
            round--; // retry
            continue;
        }

        // ── Target phase ────────────────────────────────────────────────────
        Button target = (Button)(rand() % 4);
        lv_obj_set_style_bg_color(scr, screen_color_for(target), 0);
        lv_label_set_text(letter, btn_letters[target]);
        lv_obj_center(letter);
        lv_timer_handler();

        // Light LEDs to match target color
        uint8_t r, g, b;
        led_color_for(target, &r, &g, &b);
        led_all(r, g, b);

        play_target_beep();

        uint32_t show_time = to_ms_since_boot(get_absolute_time());
        uint32_t reaction = 0;
        bool correct = false;
        int pressed = -1;

        while (to_ms_since_boot(get_absolute_time()) - show_time < 3000)
        {
            pressed = poll_pressed();
            if (pressed != -1)
            {
                reaction = to_ms_since_boot(get_absolute_time()) - show_time;
                correct = (pressed == (int)target);
                break;
            }
            lv_timer_handler();
            sleep_ms(2);
        }

        // ── Result phase ────────────────────────────────────────────────────
        lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
        char buf[32];

        if (reaction == 0)
        {
            // Timeout
            lv_label_set_text(letter, "Too slow!");
            lv_obj_center(letter);
            lv_timer_handler();
            play_error();
            led_flash_red();
        }
        else if (!correct)
        {
            // Wrong button
            snprintf(buf, sizeof(buf), "Wrong!\n%lu ms", reaction);
            lv_label_set_text(letter, buf);
            lv_obj_center(letter);
            lv_timer_handler();
            play_error();
            led_flash_red();
        }
        else
        {
            // Correct
            snprintf(buf, sizeof(buf), "%lu ms", reaction);
            lv_label_set_text(letter, buf);
            lv_obj_center(letter);
            lv_timer_handler();
            play_ding();
            led_flash_green();
            sleep_ms(800);
            led_off();
            times[valid++] = reaction;
        }
    }

    // ── Final summary ──────────────────────────────────────────────────────
    lv_label_set_text(round_label, "");
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    led_off();

    if (valid > 0)
    {
        uint32_t sum = 0, best = UINT32_MAX;
        for (int i = 0; i < valid; i++)
        {
            sum += times[i];
            if (times[i] < best)
                best = times[i];
        }
        uint32_t avg = sum / valid;

        char buf[64];
        snprintf(buf, sizeof(buf), "Avg: %lu ms\nBest: %lu ms", avg, best);
        lv_label_set_text(letter, buf);
    }
    else
    {
        lv_label_set_text(letter, "No valid rounds");
    }
    lv_obj_center(letter);
    lv_timer_handler();

    // Wait for joystick click to exit
    while (!joystick_sw_consume())
    {
        lv_timer_handler();
        sleep_ms(10);
    }

    led_off();
    lv_obj_clean(scr);
}