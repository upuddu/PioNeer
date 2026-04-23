#include "car_game.h"
#include "gpio_buttons.h"
#include "adc_joystick.h"
#include "audio.h"
#include "config.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Game constants
#define LANE_WIDTH 80
#define ROAD_WIDTH (LANE_WIDTH * 3)
#define GRASS_WIDTH ((LCD_WIDTH - ROAD_WIDTH) / 2)
#define CAR_WIDTH 40
#define CAR_HEIGHT 60
#define MAX_OBSTACLES 6

typedef enum {
    GAME_STATE_RUNNING,
    GAME_STATE_GAME_OVER
} GameState;

typedef struct {
    lv_obj_t *obj;
    int lane;
    float y;
    bool active;
    bool is_debris;
} Obstacle;

typedef struct {
    GameState state;
    lv_obj_t *screen;
    lv_obj_t *road;
    lv_obj_t *player;
    lv_obj_t *score_label;
    lv_obj_t *lives_label;
    lv_obj_t *game_over_panel;
    
    lv_obj_t *dividers[6]; // lane dividers for scrolling
    
    int player_lane;
    int lives;
    int score;
    float speed;
    int invincibility;
    
    Obstacle obstacles[MAX_OBSTACLES];
} CarGameState;

static CarGameState game;

// Core 1 SFX (Keep the same as it was working)
volatile int car_pending_sfx = 0; 
static void car_play_sfx(int sfx_id) { car_pending_sfx = sfx_id; }
static void car_sfx_core1_entry(void) {
    while(1) {
        int sfx = car_pending_sfx;
        if (sfx != 0) car_pending_sfx = 0;
        if (sfx == 1) { set_freq(0, 400); sleep_ms(150); set_freq(0, 0); }
        else if (sfx == 2) { for(float f=200; f>50; f-=10){set_freq(0,f);sleep_ms(10);} set_freq(0,0); }
        else if (sfx == 3) { for(float f=1500; f>500; f-=100){set_freq(0,f);sleep_ms(10);} set_freq(0,0); }
        else if (sfx == 4) { for(float f=300; f<800; f+=20){set_freq(0,f);sleep_ms(10);} set_freq(0,0); }
        else if (sfx == 5) { set_freq(0, 200); sleep_ms(300); set_freq(0, 100); sleep_ms(500); set_freq(0, 0); }
        else sleep_ms(20);
    }
}

static void update_hud(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE: %d", game.score / 10);
    lv_label_set_text(game.score_label, buf);
    snprintf(buf, sizeof(buf), "LIVES: %d", game.lives);
    lv_label_set_text(game.lives_label, buf);
}

void car_button_callback(uint8_t btn, bool pressed) {
    if (!pressed) return;
    if (game.state == GAME_STATE_GAME_OVER) {
        lv_obj_clean(lv_screen_active());
        car_game_init();
        return;
    }
    if (btn == BTN_A) car_play_sfx(1);
    else if (btn == BTN_X) { game.speed = 12.0f; game.invincibility = 60; car_play_sfx(4); }
}

void car_game_init(void) {
    memset(&game, 0, sizeof(CarGameState));
    
    printf("[CAR] Initializing with standard LVGL objects...\n");
    game.state = GAME_STATE_RUNNING;
    game.player_lane = 1;
    game.lives = 3;
    game.speed = 5.0f;
    
    // Main Screen Background (Grass)
    game.screen = lv_screen_active();
    lv_obj_set_style_bg_color(game.screen, lv_color_hex(0x228B22), 0);
    lv_obj_set_style_bg_opa(game.screen, LV_OPA_COVER, 0);
    
    // Road
    game.road = lv_obj_create(game.screen);
    lv_obj_set_size(game.road, ROAD_WIDTH, LCD_HEIGHT);
    lv_obj_align(game.road, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(game.road, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(game.road, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(game.road, 0, 0);
    lv_obj_remove_flag(game.road, LV_OBJ_FLAG_SCROLLABLE);

    // Lane Dividers
    for (int i = 0; i < 6; i++) {
        game.dividers[i] = lv_obj_create(game.road);
        lv_obj_set_size(game.dividers[i], 4, 40);
        lv_obj_set_style_bg_color(game.dividers[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(game.dividers[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(game.dividers[i], 0, 0);
        int lane_idx = (i < 3) ? 1 : 2;
        lv_obj_set_pos(game.dividers[i], lane_idx * LANE_WIDTH - 2, (i % 3) * 120);
    }

    // Player Car
    game.player = lv_obj_create(game.road);
    lv_obj_set_size(game.player, CAR_WIDTH, CAR_HEIGHT);
    lv_obj_set_style_bg_color(game.player, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(game.player, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(game.player, 5, 0);
    lv_obj_set_style_border_width(game.player, 2, 0);
    lv_obj_set_style_border_color(game.player, lv_color_black(), 0);
    
    // HUD
    game.score_label = lv_label_create(game.screen);
    lv_obj_set_style_text_color(game.score_label, lv_color_white(), 0);
    lv_obj_set_pos(game.score_label, 10, 10);

    game.lives_label = lv_label_create(game.screen);
    lv_obj_set_style_text_color(game.lives_label, lv_color_hex(0xFF8888), 0);
    lv_obj_set_pos(game.lives_label, 10, 40);
    
    // Game Over Panel (Hidden initially)
    game.game_over_panel = lv_obj_create(game.screen);
    lv_obj_set_size(game.game_over_panel, 300, 150);
    lv_obj_center(game.game_over_panel);
    lv_obj_set_style_bg_color(game.game_over_panel, lv_color_black(), 0);
    lv_obj_add_flag(game.game_over_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *go_text = lv_label_create(game.game_over_panel);
    lv_label_set_text(go_text, "GAME OVER\nPress any button");
    lv_obj_set_style_text_color(go_text, lv_color_white(), 0);
    lv_obj_center(go_text);

    // Obstacles
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        game.obstacles[i].obj = lv_obj_create(game.road);
        lv_obj_set_size(game.obstacles[i].obj, CAR_WIDTH, CAR_HEIGHT);
        lv_obj_set_style_border_width(game.obstacles[i].obj, 0, 0);
        lv_obj_add_flag(game.obstacles[i].obj, LV_OBJ_FLAG_HIDDEN);
        game.obstacles[i].active = false;
    }

    static bool core1_launched = false;
    if (!core1_launched) {
        set_waveform(WAVE_SQUARE);
        multicore_launch_core1(car_sfx_core1_entry);
        core1_launched = true;
    }
    update_hud();
}

void car_game_update(void) {
    if (game.state != GAME_STATE_RUNNING) return;

    // Movement
    JoystickReading joy = joystick_read();
    int dx = (int)joy.x - 2048;
    static int joy_lock = 0;
    if (abs(dx) < 500) joy_lock = 0;
    else if (!joy_lock) {
        if (dx < -1000 && game.player_lane > 0) { game.player_lane--; joy_lock = 1; }
        else if (dx > 1000 && game.player_lane < 2) { game.player_lane++; joy_lock = 1; }
    }

    int px = game.player_lane * LANE_WIDTH + (LANE_WIDTH - CAR_WIDTH) / 2;
    lv_obj_set_pos(game.player, px, 240);

    // Scrolling
    game.score++;
    if (game.invincibility > 0) {
        game.invincibility--;
        lv_obj_set_style_opa(game.player, (game.invincibility % 4 < 2) ? LV_OPA_50 : LV_OPA_COVER, 0);
        if (game.invincibility == 0) game.speed = 5.0f;
    } else {
        lv_obj_set_style_opa(game.player, LV_OPA_COVER, 0);
    }

    for (int i = 0; i < 6; i++) {
        int y = lv_obj_get_y(game.dividers[i]);
        y += (int)game.speed;
        if (y > LCD_HEIGHT) y = -40;
        lv_obj_set_y(game.dividers[i], y);
    }

    // Spawn & Update Obstacles
    if (rand() % 100 < 3) {
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            if (!game.obstacles[i].active) {
                game.obstacles[i].active = true;
                game.obstacles[i].lane = rand() % 3;
                game.obstacles[i].y = -100;
                game.obstacles[i].is_debris = (rand() % 10 < 3);
                
                lv_obj_remove_flag(game.obstacles[i].obj, LV_OBJ_FLAG_HIDDEN);
                if (game.obstacles[i].is_debris) {
                    lv_obj_set_size(game.obstacles[i].obj, 25, 25);
                    lv_obj_set_style_bg_color(game.obstacles[i].obj, lv_color_hex(0x8B4513), 0);
                } else {
                    lv_obj_set_size(game.obstacles[i].obj, CAR_WIDTH, CAR_HEIGHT);
                    lv_obj_set_style_bg_color(game.obstacles[i].obj, lv_color_hex(0x0000FF), 0);
                }
                break;
            }
        }
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!game.obstacles[i].active) continue;
        game.obstacles[i].y += game.speed + (game.obstacles[i].is_debris ? 0 : 2);
        int ox = game.obstacles[i].lane * LANE_WIDTH + (LANE_WIDTH - lv_obj_get_width(game.obstacles[i].obj)) / 2;
        lv_obj_set_pos(game.obstacles[i].obj, ox, (int)game.obstacles[i].y);

        if (game.obstacles[i].y > LCD_HEIGHT) {
            game.obstacles[i].active = false;
            lv_obj_add_flag(game.obstacles[i].obj, LV_OBJ_FLAG_HIDDEN);
        } else if (game.invincibility == 0) {
            // Collision Check
            if (game.player_lane == game.obstacles[i].lane) {
                if (game.obstacles[i].y + lv_obj_get_height(game.obstacles[i].obj) > 240 && game.obstacles[i].y < 300) {
                    game.lives--;
                    game.invincibility = 60;
                    car_play_sfx(2);
                    game.obstacles[i].active = false;
                    lv_obj_add_flag(game.obstacles[i].obj, LV_OBJ_FLAG_HIDDEN);
                    update_hud();
                    if (game.lives <= 0) {
                        game.state = GAME_STATE_GAME_OVER;
                        lv_obj_remove_flag(game.game_over_panel, LV_OBJ_FLAG_HIDDEN);
                        car_play_sfx(5);
                    }
                }
            }
        }
    }
    update_hud();
}
