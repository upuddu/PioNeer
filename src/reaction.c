#include "reaction.h"
#include "gpio_buttons.h"
#include "adc_joystick.h"
#include "lvgl.h"
#include "pico/stdlib.h"
#include <stdlib.h>
#include <stdio.h>

#define NUM_ROUNDS 5

// Colors matching button positions
// X (top-left)   A (top-right)
// Y (bottom-left) B (bottom-right)
#define COLOR_BG lv_color_hex(0x111111)
#define COLOR_X lv_color_hex(0x2196F3) // blue
#define COLOR_A lv_color_hex(0x4CAF50) // green
#define COLOR_Y lv_color_hex(0xFFC107) // yellow
#define COLOR_B lv_color_hex(0xF44336) // red
#define COLOR_TEXT lv_color_hex(0xFFFFFF)
#define COLOR_WAIT lv_color_hex(0x424242)

static const char *btn_letters[] = {"A", "B", "X", "Y"};

static lv_color_t color_for(Button b)
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

// Check if a specific button was just pressed
// Returns BTN_A/B/X/Y if pressed this frame, or -1 if none
static int poll_pressed(void)
{
    static ButtonState prev[4] = {BTN_STATE_RELEASED, BTN_STATE_RELEASED, BTN_STATE_RELEASED, BTN_STATE_RELEASED};
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

void reaction_run(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    // Big center letter
    lv_obj_t *letter = lv_label_create(scr);
    lv_obj_set_style_text_color(letter, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(letter, &lv_font_montserrat_32, 0);
    lv_label_set_text(letter, "Ready?");
    lv_obj_center(letter);

    // Status label at top
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

    lv_timer_handler();
    sleep_ms(1500);

    uint32_t times[NUM_ROUNDS];
    int valid = 0;

    for (int round = 0; round < NUM_ROUNDS; round++)
    {
        char rbuf[16];
        snprintf(rbuf, sizeof(rbuf), "Round %d/%d", round + 1, NUM_ROUNDS);
        lv_label_set_text(round_label, rbuf);

        // Wait screen
        lv_obj_set_style_bg_color(scr, COLOR_WAIT, 0);
        lv_label_set_text(letter, "...");
        lv_obj_center(letter);
        lv_timer_handler();

        // Random wait 1.5s – 4s
        uint32_t wait_ms = 1500 + (rand() % 2500);
        uint32_t start_wait = to_ms_since_boot(get_absolute_time());

        // During wait, detect early press = foul
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
            sleep_ms(1500);
            round--; // retry this round
            continue;
        }

        // Show target button
        Button target = (Button)(rand() % 4);
        lv_obj_set_style_bg_color(scr, color_for(target), 0);
        lv_label_set_text(letter, btn_letters[target]);
        lv_obj_center(letter);
        lv_timer_handler();

        uint32_t show_time = to_ms_since_boot(get_absolute_time());
        uint32_t reaction = 0;
        bool correct = false;

        // Wait up to 3 seconds for response
        while (to_ms_since_boot(get_absolute_time()) - show_time < 3000)
        {
            int pressed = poll_pressed();
            if (pressed != -1)
            {
                reaction = to_ms_since_boot(get_absolute_time()) - show_time;
                correct = (pressed == (int)target);
                break;
            }
            lv_timer_handler();
            sleep_ms(2);
        }

        // Show result
        lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
        char buf[32];
        if (reaction == 0)
        {
            lv_label_set_text(letter, "Too slow!");
        }
        else if (!correct)
        {
            snprintf(buf, sizeof(buf), "Wrong!\n%lu ms", reaction);
            lv_label_set_text(letter, buf);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%lu ms", reaction);
            lv_label_set_text(letter, buf);
            times[valid++] = reaction;
        }
        lv_obj_center(letter);
        lv_timer_handler();
        sleep_ms(1500);
    }

    // Final results
    lv_label_set_text(round_label, "");
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

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

    lv_obj_clean(scr);
}