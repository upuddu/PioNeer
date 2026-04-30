#include "audio_maze.h"
#include "gpio_buttons.h"
#include "adc_joystick.h"
#include "audio.h"
#include "config.h"
#include "led_strip.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define W 0
#define E 1
#define R 2
#define S 3
#define G 4

static const uint8_t maze[10][15] = {
    {W, W, W, W, W, W, W, W, W, W, W, W, W, W, W}, // 0
    {W, S, R, R, W, E, E, E, E, W, E, E, G, R, W}, // 1
    {W, W, W, R, W, W, E, W, E, W, W, W, W, R, W}, // 2
    {W, E, W, R, R, R, R, W, E, E, R, R, R, R, W}, // 3
    {W, E, W, W, W, W, R, W, W, W, R, W, W, W, W}, // 4
    {W, E, E, E, E, W, R, R, R, R, R, W, E, E, W}, // 5
    {W, W, W, W, E, W, W, W, W, W, W, W, W, E, W}, // 6
    {W, E, E, W, E, E, E, E, E, E, E, E, E, E, W}, // 7
    {W, E, E, E, E, W, W, W, W, W, W, W, W, W, W}, // 8
    {W, W, W, W, W, W, W, W, W, W, W, W, W, W, W}  // 9
};

volatile int maze_player_state = 0; // 0=wrong, 1=right, 2=goal
volatile float maze_wall_dist = 100.0f;

static float player_x = 48.0f;
static float player_y = 48.0f;
static int illumination_timer = 0;

static lv_obj_t *screen;
static lv_obj_t *player_obj;
static lv_obj_t *overlay;
static lv_obj_t *win_label;

static void audio_maze_core1_entry(void) {
    uint32_t t = 0;
    while(1) {
        if (maze_player_state == 2) {
            // Goal!
            set_freq(0, 440); set_freq(1, 0); sleep_ms(150);
            set_freq(0, 554); sleep_ms(150);
            set_freq(0, 659); sleep_ms(300);
            set_freq(0, 0); sleep_ms(1000);
            continue;
        }
        
        // Channel 0: Wall hum
        if (maze_wall_dist < 64.0f) {
            int toggle_period = (int)(maze_wall_dist / 2.0f) + 2; // 2 to 34 (x10 ms)
            if ((t / toggle_period) % 2 == 0) {
                set_freq(0, 50.0f);
            } else {
                set_freq(0, 75.0f);
            }
        } else {
            set_freq(0, 0.0f);
        }
        
        // Channel 1: Right/Wrong path
        if (maze_player_state == 1) {
            // Right path melodic tune (changes every 250ms)
            int note_idx = (t / 25) % 4; 
            float notes[] = {440.0f, 554.0f, 659.0f, 554.0f};
            set_freq(1, notes[note_idx]);
        } else {
            // Wrong path light buzz
            if (t % 2 == 0) set_freq(1, 100.0f);
            else set_freq(1, 120.0f);
        }
        
        t++;
        sleep_ms(10);
    }
}

void audio_maze_init(void) {
    printf("[AUDIO MAZE] Initializing...\n");
    player_x = 48.0f; // Tile (1,1) center
    player_y = 48.0f;
    illumination_timer = 0;
    maze_player_state = 0;
    maze_wall_dist = 100.0f;

    screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // Draw the maze tiles
    for (int ty = 0; ty < 10; ty++) {
        for (int tx = 0; tx < 15; tx++) {
            lv_obj_t *tile = lv_obj_create(screen);
            lv_obj_set_size(tile, 32, 32);
            lv_obj_set_pos(tile, tx * 32, ty * 32);
            lv_obj_set_style_border_width(tile, 1, 0);
            lv_obj_set_style_border_color(tile, lv_color_hex(0x222222), 0);
            lv_obj_set_style_radius(tile, 0, 0);
            lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

            if (maze[ty][tx] == W) {
                lv_obj_set_style_bg_color(tile, lv_color_hex(0x333333), 0); // Wall
            } else if (maze[ty][tx] == G) {
                lv_obj_set_style_bg_color(tile, lv_color_hex(0x00FF00), 0); // Goal
            } else if (maze[ty][tx] == S) {
                lv_obj_set_style_bg_color(tile, lv_color_hex(0x0000FF), 0); // Start
            } else {
                lv_obj_set_style_bg_color(tile, lv_color_hex(0x555555), 0); // Path
            }
        }
    }

    // Draw player
    player_obj = lv_obj_create(screen);
    lv_obj_set_size(player_obj, 16, 16);
    lv_obj_set_style_bg_color(player_obj, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(player_obj, 8, 0); // Circle
    lv_obj_set_style_border_width(player_obj, 0, 0);
    lv_obj_remove_flag(player_obj, LV_OBJ_FLAG_SCROLLABLE);

    // Overlay to obscure vision
    overlay = lv_obj_create(screen);
    lv_obj_set_size(overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);

    // Win Label
    win_label = lv_label_create(screen);
    lv_label_set_text(win_label, "GOAL REACHED!");
    lv_obj_set_style_text_color(win_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(win_label, &lv_font_montserrat_14, 0);
    lv_obj_center(win_label);
    lv_obj_add_flag(win_label, LV_OBJ_FLAG_HIDDEN);

    static bool core1_launched = false;
    if (!core1_launched) {
        set_waveform(WAVE_SINE);
        multicore_launch_core1(audio_maze_core1_entry);
        core1_launched = true;
    }
}

static bool check_collision(float px, float py) {
    int tx1 = (int)(px - 8) / 32;
    int tx2 = (int)(px + 8) / 32;
    int ty1 = (int)(py - 8) / 32;
    int ty2 = (int)(py + 8) / 32;
    
    for (int ty = ty1; ty <= ty2; ty++) {
        for (int tx = tx1; tx <= tx2; tx++) {
            if (tx >= 0 && tx < 15 && ty >= 0 && ty < 10) {
                if (maze[ty][tx] == W) return true;
            } else {
                return true; // Out of bounds
            }
        }
    }
    return false;
}

void audio_maze_update(void) {
    if (maze_player_state == 2) return; // Goal reached, stop game

    JoystickReading joy = joystick_read();
    float dx = ((int)joy.x - 2048) / 2048.0f;
    float dy = ((int)joy.y - 2048) / 2048.0f;
    
    // Deadzone
    if (fabs(dx) < 0.15f) dx = 0;
    if (fabs(dy) < 0.15f) dy = 0;

    float speed = 3.0f;
    float nx = player_x + dx * speed;
    float ny = player_y + dy * speed;

    // Movement X
    if (!check_collision(nx, player_y)) {
        player_x = nx;
    }
    // Movement Y
    if (!check_collision(player_x, ny)) {
        player_y = ny;
    }

    lv_obj_set_pos(player_obj, (int)player_x - 8, (int)player_y - 8);

    // Current tile state
    int p_tx = (int)(player_x) / 32;
    int p_ty = (int)(player_y) / 32;
    uint8_t current_tile = W;
    if (p_tx >= 0 && p_tx < 15 && p_ty >= 0 && p_ty < 10) {
        current_tile = maze[p_ty][p_tx];
    }

    static uint8_t last_tile = 255;
    if ((current_tile == R || current_tile == S) && (last_tile != R && last_tile != S)) {
        // Just entered a right path, illuminate!
        illumination_timer = 50; // 50 frames
    }
    last_tile = current_tile;

    if (current_tile == G) {
        maze_player_state = 2;
        lv_obj_remove_flag(win_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_0, 0); // Reveal maze on win
    } else if (current_tile == R || current_tile == S) {
        maze_player_state = 1;
    } else {
        maze_player_state = 0;
    }

    // Handle illumination fade out
    static bool led_was_on = false;
    if (maze_player_state != 2) {
        if (illumination_timer > 0) {
            illumination_timer--;
            // Target opacity: from 100 (visible) to 255 (black)
            int opa = 255 - (illumination_timer * 4); 
            if (opa < 100) opa = 100;
            if (opa > 255) opa = 255;
            lv_obj_set_style_bg_opa(overlay, opa, 0);

            if (!led_was_on) {
                led_strip_set_all(20, 20, 20); // Dull white
                led_strip_show();
                led_was_on = true;
            }
        } else {
            lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);

            if (led_was_on) {
                led_strip_clear();
                led_strip_show();
                led_was_on = false;
            }
        }
    } else {
        // Goal reached LED
        if (!led_was_on) {
            led_strip_set_all_color(COLOR_GREEN);
            led_strip_show();
            led_was_on = true;
        }
    }

    // Calculate distance to nearest wall
    float min_dist = 9999.0f;
    for (int ty = 0; ty < 10; ty++) {
        for (int tx = 0; tx < 15; tx++) {
            if (maze[ty][tx] == W) {
                float clamp_x = player_x;
                if (clamp_x < tx * 32.0f) clamp_x = tx * 32.0f;
                if (clamp_x > tx * 32.0f + 32.0f) clamp_x = tx * 32.0f + 32.0f;
                float clamp_y = player_y;
                if (clamp_y < ty * 32.0f) clamp_y = ty * 32.0f;
                if (clamp_y > ty * 32.0f + 32.0f) clamp_y = ty * 32.0f + 32.0f;
                
                float d = sqrtf((player_x - clamp_x)*(player_x - clamp_x) + (player_y - clamp_y)*(player_y - clamp_y));
                if (d < min_dist) min_dist = d;
            }
        }
    }
    maze_wall_dist = min_dist;
}

void audio_maze_button_callback(uint8_t btn, bool pressed) {
    if (!pressed) return;
    if (maze_player_state == 2) {
        // Restart on win
        led_strip_clear();
        led_strip_show();
        audio_maze_init();
    }
}
