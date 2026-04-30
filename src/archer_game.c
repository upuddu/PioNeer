#include "archer_game.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "audio.h"
#include "led_strip.h"
#include "config.h"
#include "lvgl/lvgl.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef enum {
    STATE_P1_START,
    STATE_P1_PLAY,
    STATE_P2_START,
    STATE_P2_PLAY,
    STATE_GAME_OVER
} ArcherState;

static ArcherState current_state = STATE_P1_START;
static int p1_score = 0;
static int p2_score = 0;
static uint32_t turn_start_time = 0;
static const uint32_t TURN_DURATION_MS = 30000; // 30 seconds

static float cursor_x = LCD_WIDTH / 2.0f;
static float cursor_y = LCD_HEIGHT / 2.0f;
static float vel_x = 0;
static float vel_y = 0;
static float target_x = 0;
static float target_y = 0;

static int dwell_frames = 0;
static const int DWELL_MAX = 75; // ~1.5 seconds at 50fps loop
static bool is_drawing = false;

volatile int archer_audio_state = 0; // 0=idle, 1=pulling, 2=hit, 3=win
volatile int archer_dwell_progress = 0;

static lv_obj_t *screen;
static lv_obj_t *cursor_obj;
static lv_obj_t *target_obj;
static lv_obj_t *info_label;
static lv_obj_t *timer_label;
static lv_obj_t *score_label;

static void archer_core1_entry(void) {
    while(1) {
        if (archer_audio_state == 3) {
            set_freq(0, 440); sleep_ms(150); set_freq(0, 554); sleep_ms(150); set_freq(0, 659); sleep_ms(500);
            set_freq(0, 0); sleep_ms(1000);
            archer_audio_state = 0;
        } else if (archer_audio_state == 2) {
            set_freq(0, 1500);
            sleep_ms(100);
            set_freq(0, 0);
            archer_audio_state = 0;
        } else if (archer_audio_state == 1) {
            float freq = 200.0f + (archer_dwell_progress * 8.0f);
            set_freq(0, freq);
            sleep_ms(10);
        } else {
            set_freq(0, 0);
            sleep_ms(20);
        }
    }
}

static void spawn_target(void) {
    target_x = 40 + (rand() % (LCD_WIDTH - 80));
    target_y = 60 + (rand() % (LCD_HEIGHT - 100)); // leave room for top HUD
    lv_obj_set_pos(target_obj, (int)target_x - 20, (int)target_y - 20);
    lv_obj_remove_flag(target_obj, LV_OBJ_FLAG_HIDDEN);
    dwell_frames = 0;
}

static void update_hud(void) {
    char buf[64];
    if (current_state == STATE_P1_PLAY) {
        snprintf(buf, sizeof(buf), "P1 Score: %d", p1_score);
        lv_label_set_text(score_label, buf);
        uint32_t remaining = (TURN_DURATION_MS - (lv_tick_get() - turn_start_time)) / 1000;
        snprintf(buf, sizeof(buf), "Time: %lds", remaining);
        lv_label_set_text(timer_label, buf);
    } else if (current_state == STATE_P2_PLAY) {
        snprintf(buf, sizeof(buf), "P2 Score: %d", p2_score);
        lv_label_set_text(score_label, buf);
        uint32_t remaining = (TURN_DURATION_MS - (lv_tick_get() - turn_start_time)) / 1000;
        snprintf(buf, sizeof(buf), "Time: %lds", remaining);
        lv_label_set_text(timer_label, buf);
    }
}

void archer_game_init(void) {
    printf("[ARCHER] Init\n");
    current_state = STATE_P1_START;
    p1_score = 0;
    p2_score = 0;
    cursor_x = LCD_WIDTH / 2.0f;
    cursor_y = LCD_HEIGHT / 2.0f;
    vel_x = 0;
    vel_y = 0;
    dwell_frames = 0;
    archer_audio_state = 0;
    is_drawing = false;

    screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x228B22), 0); // Forest green
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    info_label = lv_label_create(screen);
    lv_label_set_text(info_label, "Player 1 Ready!\nPress any button to start.");
    lv_obj_set_style_text_color(info_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);

    score_label = lv_label_create(screen);
    lv_label_set_text(score_label, "P1 Score: 0");
    lv_obj_set_style_text_color(score_label, lv_color_white(), 0);
    lv_obj_set_pos(score_label, 10, 10);
    lv_obj_add_flag(score_label, LV_OBJ_FLAG_HIDDEN);

    timer_label = lv_label_create(screen);
    lv_label_set_text(timer_label, "Time: 30s");
    lv_obj_set_style_text_color(timer_label, lv_color_white(), 0);
    lv_obj_align(timer_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_flag(timer_label, LV_OBJ_FLAG_HIDDEN);

    target_obj = lv_obj_create(screen);
    lv_obj_set_size(target_obj, 40, 40);
    lv_obj_set_style_radius(target_obj, 20, 0);
    lv_obj_set_style_bg_color(target_obj, lv_color_hex(0xFF0000), 0); // Red
    lv_obj_set_style_border_color(target_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(target_obj, 4, 0);
    lv_obj_add_flag(target_obj, LV_OBJ_FLAG_HIDDEN);

    cursor_obj = lv_obj_create(screen);
    lv_obj_set_size(cursor_obj, 20, 20);
    lv_obj_set_style_radius(cursor_obj, 10, 0);
    lv_obj_set_style_bg_color(cursor_obj, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_bg_opa(cursor_obj, LV_OPA_50, 0); // Semi-transparent
    lv_obj_set_style_border_color(cursor_obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(cursor_obj, 2, 0);
    lv_obj_add_flag(cursor_obj, LV_OBJ_FLAG_HIDDEN);

    static bool core1_started = false;
    if (!core1_started) {
        set_waveform(WAVE_TRIANGLE);
        multicore_launch_core1(archer_core1_entry);
        core1_started = true;
    }
    led_strip_clear();
    led_strip_show();
}

void archer_game_update(void) {
    if (current_state == STATE_GAME_OVER) {
        // Rainbow effect for winner
        static uint32_t last_anim = 0;
        if (lv_tick_get() - last_anim > 50) {
            led_strip_shift_left();
            led_strip_show();
            last_anim = lv_tick_get();
        }
        return;
    }

    if (current_state == STATE_P1_START || current_state == STATE_P2_START) {
        return; // Waiting
    }

    // Play states
    uint32_t elapsed = lv_tick_get() - turn_start_time;
    if (elapsed >= TURN_DURATION_MS) {
        // Turn over
        archer_audio_state = 0;
        led_strip_clear();
        led_strip_show();
        lv_obj_add_flag(target_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cursor_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(score_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(timer_label, LV_OBJ_FLAG_HIDDEN);

        if (current_state == STATE_P1_PLAY) {
            current_state = STATE_P2_START;
            lv_label_set_text(info_label, "Player 1 Turn Over!\nPlayer 2 Ready!\nPress any button to start.");
            lv_obj_remove_flag(info_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            current_state = STATE_GAME_OVER;
            char buf[128];
            if (p1_score > p2_score) snprintf(buf, sizeof(buf), "GAME OVER\nPlayer 1 Wins!\n%d to %d\nPress button to restart", p1_score, p2_score);
            else if (p2_score > p1_score) snprintf(buf, sizeof(buf), "GAME OVER\nPlayer 2 Wins!\n%d to %d\nPress button to restart", p2_score, p1_score);
            else snprintf(buf, sizeof(buf), "GAME OVER\nIt's a Tie!\n%d to %d\nPress button to restart", p1_score, p2_score);
            lv_label_set_text(info_label, buf);
            lv_obj_remove_flag(info_label, LV_OBJ_FLAG_HIDDEN);
            
            // Start rainbow
            led_strip_fill_rainbow(0, 25);
            led_strip_show();
            archer_audio_state = 3;
        }
        return;
    }

    update_hud();

    // Movement
    JoystickReading joy = joystick_read();
    float dx = ((int)joy.x - 2048) / 2048.0f;
    float dy = ((int)joy.y - 2048) / 2048.0f;
    
    if (fabs(dx) < 0.1f) dx = 0;
    if (fabs(dy) < 0.1f) dy = 0;

    vel_x += dx * 0.5f;
    vel_y += dy * 0.5f;

    float speed = sqrtf(vel_x*vel_x + vel_y*vel_y);
    if (speed > 8.0f) {
        vel_x = (vel_x / speed) * 8.0f;
        vel_y = (vel_y / speed) * 8.0f;
    }

    vel_x *= 0.98f;
    vel_y *= 0.98f;

    cursor_x += vel_x;
    cursor_y += vel_y;

    if (cursor_x < 10) { cursor_x = 10; vel_x = -vel_x * 0.5f; }
    if (cursor_x > LCD_WIDTH - 10) { cursor_x = LCD_WIDTH - 10; vel_x = -vel_x * 0.5f; }
    if (cursor_y < 10) { cursor_y = 10; vel_y = -vel_y * 0.5f; }
    if (cursor_y > LCD_HEIGHT - 10) { cursor_y = LCD_HEIGHT - 10; vel_y = -vel_y * 0.5f; }

    lv_obj_set_pos(cursor_obj, (int)cursor_x - 10, (int)cursor_y - 10);

    // Dwell check replaced by manual aim and draw check
    if (is_drawing) {
        if (dwell_frames < DWELL_MAX) {
            dwell_frames++;
        }
        if (archer_audio_state == 0) archer_audio_state = 1; // start pulling
        archer_dwell_progress = dwell_frames;

        // Update LEDs - brighter and more interaction
        int leds_on = (dwell_frames * WS2812_NUM_LEDS) / DWELL_MAX;
        if (leds_on > WS2812_NUM_LEDS) leds_on = WS2812_NUM_LEDS;
        led_strip_clear();
        for (int i = 0; i < leds_on; i++) {
            // Brighter LEDs: scale color intensity
            uint8_t intensity = 50 + (205 * dwell_frames) / DWELL_MAX;
            uint32_t color = (intensity << 16); // Red
            if (dwell_frames >= DWELL_MAX) {
                // When fully charged, flash yellow/white occasionally
                if (lv_tick_get() % 100 < 50) color = 0xFFFFFF; // Brighter White flash
                else color = 0xFFFF00; // Bright Yellow
            }
            led_strip_set_pixel_color(i, color);
        }
        led_strip_show();
    } else {
        if (dwell_frames > 0) {
            dwell_frames = 0;
            archer_audio_state = 0;
            led_strip_clear();
            led_strip_show();
        }
    }
}

void archer_button_callback(uint8_t btn, bool pressed) {
    if (current_state == STATE_P1_START) {
        if (!pressed) return;
        current_state = STATE_P1_PLAY;
        turn_start_time = lv_tick_get();
        lv_obj_add_flag(info_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(score_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(timer_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(cursor_obj, LV_OBJ_FLAG_HIDDEN);
        spawn_target();
    } else if (current_state == STATE_P2_START) {
        if (!pressed) return;
        current_state = STATE_P2_PLAY;
        turn_start_time = lv_tick_get();
        lv_obj_add_flag(info_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(score_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(timer_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(cursor_obj, LV_OBJ_FLAG_HIDDEN);
        spawn_target();
    } else if (current_state == STATE_GAME_OVER) {
        if (!pressed) return;
        archer_game_init();
    } else if (current_state == STATE_P1_PLAY || current_state == STATE_P2_PLAY) {
        if (pressed) {
            is_drawing = true;
        } else {
            if (is_drawing) {
                is_drawing = false;
                if (dwell_frames >= DWELL_MAX / 3) { // Require at least 1/3 charge
                    float dist = sqrtf((cursor_x - target_x)*(cursor_x - target_x) + (cursor_y - target_y)*(cursor_y - target_y));
                    if (dist < 30.0f) {
                        if (current_state == STATE_P1_PLAY) p1_score++;
                        else p2_score++;
                        archer_audio_state = 2; // hit
                        spawn_target();
                    } else {
                        archer_audio_state = 0; // miss
                    }
                }
                dwell_frames = 0;
                led_strip_clear();
                led_strip_show();
            }
        }
    }
}
