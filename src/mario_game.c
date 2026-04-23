#include "mario_game.h"
#include "gpio_buttons.h"
#include "audio.h"
#include "config.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ─── Forward Declarations ────────────────────────────────────────────────
static void mario_game_render(void);
static void mario_game_render_cb(lv_event_t * e);
void mario_play_sfx(int sfx_id);

// ─── Game State ──────────────────────────────────────────────────────────
static GameState game_state = GAME_STATE_INIT;
static MarioGameState mario_state = {0};
static absolute_time_t game_start_time;

// ─── Game Map Data ───────────────────────────────────────────────────────
const int mario_map[MARIO_MAP_HEIGHT][MARIO_MAP_WIDTH] = {
    {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00}, 
    {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
    {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
    {00, 21, 00, 00, 00, 20, 21, 20, 21, 20, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
    {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 32, 33, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 12, 12, 00, 00, 12, 12, 00, 00, 00, 00},
    {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 32, 33, 00, 00, 30, 31, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 12, 12, 12, 00, 00, 12, 12, 12, 00, 00, 00},
    {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 30, 31, 00, 00, 30, 31, 00, 00, 00, 00, 00, 00, 00, 00, 00, 12, 12, 12, 12, 00, 00, 12, 12, 12, 12, 00, 00},
    {11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 00, 00, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11},
    {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00}
};

// Map collision helper
static inline int get_collision(int x, int y) {
    if (x < 0 || y < 0 || x >= MARIO_MAP_WIDTH * 16 || y >= MARIO_MAP_HEIGHT * 16) return 0;
    return mario_map[y / 16][x / 16];
}
// ─── Music Note Frequencies ──────────────────────────────────────────────
const int mario_notes[] = {659, 659, 0, 659, 0, 523, 659, 0, 784};
const int mario_durations[] = {120, 120, 120, 120, 120, 120, 120, 120, 60};
static int current_note = 0;
static int note_time = 0;

// ─── Helper: Draw colored rectangle on canvas ────────────────────────────
static void draw_rect(lv_layer_t *layer, int x, int y, int w, int h, lv_color_t color)
{
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = color;
    rect_dsc.bg_opa = LV_OPA_COVER;
    lv_area_t coords = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &rect_dsc, &coords);
}

// ─── Helper: Draw text on canvas ─────────────────────────────────────────
static void draw_text(lv_layer_t *layer, int x, int y, const char *text, lv_color_t color)
{
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = color;
    label_dsc.text = text;
    lv_area_t coords = {x, y, x + 200, y + 20};
    lv_draw_label(layer, &label_dsc, &coords);
}

// ─── Audio SFX Task (Core 1) ─────────────────────────────────────────────
volatile int pending_sfx = 0; // 0=none, 1=jump, 2=powerup, 3=death, 4=coin, 5=stomp

void mario_play_sfx(int sfx_id) {
    pending_sfx = sfx_id;
}

static void sfx_core1_entry(void) {
    while(1) {
        int sfx = pending_sfx;
        if (sfx != 0) pending_sfx = 0; // Clear it to avoid re-triggering

        if (sfx == 1) { // JUMP
            for (float f = 400; f < 1200; f += 40) {
                set_freq(0, f);
                sleep_ms(10);
            }
            set_freq(0, 0);
        }
        else if (sfx == 2) { // POWERUP
            set_freq(0, 392); sleep_ms(80);
            set_freq(0, 523); sleep_ms(80);
            set_freq(0, 659); sleep_ms(80);
            set_freq(0, 784); sleep_ms(80);
            set_freq(0, 1046); sleep_ms(100);
            set_freq(0, 0);
        }
        else if (sfx == 3) { // DEATH
            set_freq(0, 800); sleep_ms(100);
            for (float f = 800; f > 200; f -= 10) {
                set_freq(0, f);  sleep_ms(10);
            }
            set_freq(0, 100); sleep_ms(200);
            set_freq(0, 0);
        }
        else if (sfx == 4) { // COIN
            set_freq(0, 987); sleep_ms(80);
            set_freq(0, 1318); sleep_ms(300);
            set_freq(0, 0);
        }
        else if (sfx == 5) { // STOMP GOOMBA
            for (float f = 1000; f > 300; f -= 100) {
                set_freq(0, f); sleep_ms(15);
            }
            set_freq(0, 0);
        }
        else {
            sleep_ms(20);
        }
    }
}


// ─── Button Callback - Receives input from main button handler ──────────
void mario_button_callback(uint8_t btn, bool pressed)
{
    switch (btn)
    {
        case BTN_A:  // Jump
            mario_state.btn_jump_pressed = pressed;
            break;
        case BTN_Y:  // Left
            mario_state.btn_left_pressed = pressed;
            break;
        case BTN_X:  // Right
            mario_state.btn_right_pressed = pressed;
            break;
        case BTN_B:  // Down
            // Test trigger for power up sound since powerups aren't natively implemented yet!
            if (pressed && !mario_state.btn_down_pressed) {
                mario_play_sfx(2); // Power Up SFX
            }
            mario_state.btn_down_pressed = pressed;
            break;
    }
}

// ─── Game Initialization ─────────────────────────────────────────────────
void mario_game_init(void)
{
    // Initialize player state
    mario_state.pos_x = 32;
    mario_state.pos_y = 80;
    mario_state.collision_x = 32;
    mario_state.collision_y = 80;
    
    mario_state.vel_f_x = 0;
    mario_state.vel_f_y = 0;
    mario_state.vel_x = 0;
    mario_state.vel_y = 0;
    mario_state.acc_x = 0;
    mario_state.friction = 0;
    mario_state.gravity = 1.0f;
    
    mario_state.col_bottom = 0;
    mario_state.col_top = 0;
    mario_state.col_right = 0;
    mario_state.col_left = 0;
    
    mario_state.jump_flag = 0;
    mario_state.jump_counter = 0;
    mario_state.sprite_id = 1;
    mario_state.time = 0;
    mario_state.time_sec = 0;
    mario_state.score = 0;
    mario_state.death = 0;
    mario_state.game_over = false;
    
    mario_state.btn_jump_pressed = false;
    mario_state.btn_left_pressed = false;
    mario_state.btn_right_pressed = false;
    mario_state.btn_down_pressed = false;

    // Create LVGL screen object for drawing game
    mario_state.canvas = lv_obj_create(lv_screen_active());
    lv_obj_set_size(mario_state.canvas, 480, 320);
    lv_obj_remove_style_all(mario_state.canvas);
    lv_obj_set_style_bg_color(mario_state.canvas, MARIO_COLOR_BLUE, 0);
    lv_obj_set_style_bg_opa(mario_state.canvas, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(mario_state.canvas, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(mario_state.canvas, mario_game_render_cb, LV_EVENT_DRAW_MAIN, NULL);

    game_start_time = get_absolute_time();
    game_state = GAME_STATE_RUNNING;

    // Launch audio processing on Core 1
    static bool core1_launched = false;
    if (!core1_launched) {
        set_waveform(WAVE_SQUARE); // Gives it a nice retro Mario sound!
        printf("[MARIO] Resetting core 1...\n");
        multicore_reset_core1();
        printf("[MARIO] Launching core 1 SFX...\n");
        multicore_launch_core1(sfx_core1_entry);
        printf("[MARIO] Core 1 running!\n");
        core1_launched = true;
    }

    printf("[MARIO] Game initialized\n");
}

// ─── Game Update Logic ───────────────────────────────────────────────────
void mario_game_update(void)
{
    if (game_state != GAME_STATE_RUNNING)
        return;

    // Update time
    mario_state.time++;
    absolute_time_t current_time = get_absolute_time();
    mario_state.time_sec = absolute_time_diff_us(game_start_time, current_time) / 1000000;
    
    // Perform rendering
    mario_game_render();

    // ─── COLLISION DETECTION ─────────────────────────────────────────────
    if (mario_state.collision_y >= 0)
    {
        // Horizontal collision checks
        mario_state.col_bottom = (get_collision(mario_state.collision_x + 1, mario_state.collision_y + 24) >= 10) ||
                                  (get_collision(mario_state.collision_x + 14, mario_state.collision_y + 24) >= 10);

        mario_state.col_top = (get_collision(mario_state.collision_x + 1, mario_state.collision_y) >= 10) ||
                              (get_collision(mario_state.collision_x + 14, mario_state.collision_y) >= 10);

        mario_state.col_right = (get_collision(mario_state.collision_x + 16, mario_state.collision_y + 23) >= 10) ||
                               (get_collision(mario_state.collision_x + 16, mario_state.collision_y + 1) >= 10);

        mario_state.col_left = (get_collision(mario_state.collision_x - 1, mario_state.collision_y + 23) >= 10) ||
                              (get_collision(mario_state.collision_x - 1, mario_state.collision_y + 1) >= 10);
    }

    // ─── MOVEMENT ────────────────────────────────────────────────────────
    if (mario_state.btn_right_pressed)
    {
        mario_state.acc_x = 1;
    }
    else if (mario_state.btn_left_pressed)
    {
        mario_state.acc_x = -1;
    }
    else
    {
        mario_state.acc_x = 0;
    }

    // Velocity cap
    if (mario_state.vel_f_x >= 4.0f || mario_state.vel_f_x <= -4.0f)
    {
        mario_state.acc_x = 0;
    }

    // Sprite animation based on velocity
    if (mario_state.vel_x == 0)
    {
        mario_state.friction = 0;
        if (mario_state.jump_flag == 0)
        {
            if (mario_state.sprite_id == 4 || mario_state.sprite_id == 6)
                mario_state.sprite_id = 2;
            else if (mario_state.sprite_id == 3 || mario_state.sprite_id == 5)
                mario_state.sprite_id = 1;
        }
    }
    else if (mario_state.vel_f_x > 0.0f)
    {
        mario_state.friction = -0.5f;
        if (mario_state.col_right)
        {
            mario_state.acc_x = 0;
            mario_state.vel_f_x = 0;
            mario_state.friction = 0;
            mario_state.collision_x -= 2;
        }

        if (mario_state.jump_flag == 0)
        {
            if (mario_state.time % 3)
                mario_state.sprite_id = 3;
            else
                mario_state.sprite_id = 1;
        }
    }
    else if (mario_state.vel_f_x < 0.0f)
    {
        mario_state.friction = 0.5f;
        if (mario_state.col_left)
        {
            mario_state.acc_x = 0;
            mario_state.vel_f_x = 0;
            mario_state.friction = 0;
            mario_state.collision_x += 2;
        }

        if (mario_state.jump_flag == 0)
        {
            if (mario_state.time % 3)
                mario_state.sprite_id = 4;
            else
                mario_state.sprite_id = 2;
        }
    }

    mario_state.vel_f_x = mario_state.vel_f_x + mario_state.acc_x + mario_state.friction;
    mario_state.vel_x = (int)roundf(mario_state.vel_f_x);
    mario_state.collision_x += mario_state.vel_x;

    // ─── JUMPING ─────────────────────────────────────────────────────────
    if (mario_state.btn_jump_pressed && mario_state.jump_flag == 0 && mario_state.col_bottom)
    {
        mario_state.jump_flag = 1;
        mario_play_sfx(1); // Jump SFX
    }

    if (mario_state.jump_flag == 1 || mario_state.vel_y < 0)
    {
        if (mario_state.sprite_id == 2 || mario_state.sprite_id == 4 || mario_state.btn_left_pressed)
            mario_state.sprite_id = 6;
        else if (mario_state.sprite_id == 1 || mario_state.sprite_id == 3 || mario_state.btn_right_pressed)
            mario_state.sprite_id = 5;
    }

    if (mario_state.jump_flag == 1)
    {
        mario_state.jump_counter++;
        if (mario_state.col_top || mario_state.jump_counter >= 3)
        {
            mario_state.jump_flag = 0;
            mario_state.jump_counter = 0;
        }
        else
        {
            mario_state.vel_y = -10;
        }
    }

    // ─── GRAVITY & COLLISION ─────────────────────────────────────────────
    if (mario_state.col_top)
    {
        mario_state.vel_y = -mario_state.vel_y / 4;
        mario_state.collision_y += 2;
    }

    if (mario_state.col_bottom && mario_state.vel_y >= 0)
    {
        mario_state.gravity = 0;
        mario_state.jump_flag = 0;
        mario_state.vel_y = 0;
    }
    else
    {
        mario_state.gravity = 1;
    }

    mario_state.vel_y = mario_state.vel_y + (int)mario_state.gravity;
    mario_state.collision_y += mario_state.vel_y;

    // Collision resolution for standing on ground
    // Floor alignment hack
    int i = 0;
    while ((get_collision(mario_state.collision_x + 4, mario_state.collision_y + 23) >= 10) ||
           (get_collision(mario_state.collision_x + 12, mario_state.collision_y + 23) >= 10))
    {
        i++;
        mario_state.collision_y--;
    }

    // ─── BOUNDARY CHECKING ───────────────────────────────────────────────
    if (mario_state.collision_y < 0)
    {
        mario_state.col_bottom = 0;
        mario_state.col_top = 0;
        mario_state.col_left = 0;
        mario_state.col_right = 0;
    }

    if (mario_state.collision_y >= 100)
    {
        mario_state.pos_y = 96;
        mario_state.vel_y = 0;
        mario_state.col_bottom = 1;
        
        if (!mario_state.game_over) {
            mario_play_sfx(3); // Death SFX
        }
        
        mario_state.death = 1;
        mario_state.game_over = true;
        game_state = GAME_STATE_GAME_OVER;
    }

    // Falling state detection
    if (!((get_collision(mario_state.collision_x + 4, mario_state.collision_y + 23) >= 10) ||
          (get_collision(mario_state.collision_x + 12, mario_state.collision_y + 23) >= 10)))
    {
        mario_state.pos_y = mario_state.collision_y;
    }

    mario_state.pos_x = mario_state.collision_x;

    // Game over condition - time limit
    if (mario_state.time_sec >= 200)
    {
        mario_state.game_over = true;
        game_state = GAME_STATE_GAME_OVER;
    }
}

// ─── Game Render Logic ───────────────────────────────────────────────────
static void mario_game_render_cb(lv_event_t * e)
{
    lv_layer_t * layer = lv_event_get_layer(e);

    // Camera system
    int camera_x = mario_state.pos_x > 96 ? mario_state.pos_x - 96 : 0;
    int draw_offset = camera_x > 0 ? -mario_state.pos_x + 96 : 0;

    // ─── Render Map Tiles ────────────────────────────────────────────────
    for (int j = 0; j < MARIO_MAP_HEIGHT - 1; j++)
    {
        for (int k = 0; k < MARIO_MAP_WIDTH; k++)
        {
            int tile_x = draw_offset + k * MARIO_TILE_SIZE;
            int tile_y = j * MARIO_TILE_SIZE;

            if (tile_x >= -MARIO_TILE_SIZE && tile_x <= MARIO_SCREEN_WIDTH)
            {
                lv_color_t tile_color;
                switch (mario_map[j][k])
                {
                    case 0:   tile_color = MARIO_COLOR_SKY; break;
                    case 10:  tile_color = MARIO_COLOR_BROWN; break;
                    case 11:  tile_color = MARIO_COLOR_GREEN; break;
                    case 12:  tile_color = lv_color_hex(0x8B7355); break;
                    case 20:  tile_color = MARIO_COLOR_RED; break;
                    case 21:  tile_color = MARIO_COLOR_RED; break;
                    case 30:  tile_color = MARIO_COLOR_GREEN; break;
                    case 31:  tile_color = MARIO_COLOR_GREEN; break;
                    case 32:  tile_color = MARIO_COLOR_GREEN; break;
                    case 33:  tile_color = MARIO_COLOR_GREEN; break;
                    default:  tile_color = MARIO_COLOR_SKY; break;
                }
                draw_rect(&layer, tile_x, tile_y, MARIO_TILE_SIZE, MARIO_TILE_SIZE, tile_color);
            }
        }
    }

    // ─── Render Mario Character ──────────────────────────────────────────
    int mario_screen_x = draw_offset + mario_state.collision_x;
    int mario_screen_y = mario_state.collision_y;

    // Draw Mario as a colored rectangle (placeholder for sprite)
    draw_rect(&layer, mario_screen_x, mario_screen_y, MARIO_SPRITE_WIDTH, 
              MARIO_SPRITE_HEIGHT, lv_color_hex(0xFF0000));

    // ─── HUD Display ─────────────────────────────────────────────────────
    char hud_text[64];
    snprintf(hud_text, sizeof(hud_text), "MARIO  TIME:%d  SCORE:%d", (int)(200 - mario_state.time_sec), mario_state.score);
    draw_text(&layer, 10, 10, hud_text, lv_color_white());

    snprintf(hud_text, sizeof(hud_text), "X:%d Y:%d  VX:%d VY:%d", mario_state.collision_x, 
             mario_state.collision_y, mario_state.vel_x, mario_state.vel_y);
    draw_text(layer, 10, 30, hud_text, lv_color_white());
}

static void mario_game_render(void)
{
    if (mario_state.canvas) {
        lv_obj_invalidate(mario_state.canvas); // Triggers LV_EVENT_DRAW_MAIN
    }
}

// ─── Get Game State ──────────────────────────────────────────────────────
GameState mario_get_state(void)
{
    return game_state;
}

void mario_set_state(GameState state)
{
    game_state = state;
}

MarioGameState* mario_get_state_data(void)
{
    return &mario_state;
}

// ─── Display Title Screen ────────────────────────────────────────────────
void mario_display_title(void)
{
    lv_obj_t *title_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(title_screen, MARIO_SCREEN_WIDTH, MARIO_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(title_screen, MARIO_COLOR_BLUE, 0);
    lv_obj_set_scrollbar_mode(title_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(title_screen, 0, 0);
    
    lv_obj_t *l1 = lv_label_create(title_screen);
    lv_label_set_text(l1, "SUPER MARIO");
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_align(l1, LV_ALIGN_CENTER, 0, -20);
    
    lv_obj_t *l2 = lv_label_create(title_screen);
    lv_label_set_text(l2, "Press any button to start");
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);
    lv_obj_align(l2, LV_ALIGN_CENTER, 0, 20);
    
    sleep_ms(3000);
}

// ─── Display Game Over Screen ────────────────────────────────────────────
void mario_display_game_over(void)
{
    lv_obj_t *go_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(go_screen, MARIO_SCREEN_WIDTH, MARIO_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(go_screen, MARIO_COLOR_BLUE, 0);
    lv_obj_set_scrollbar_mode(go_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(go_screen, 0, 0);
    
    lv_obj_t *l1 = lv_label_create(go_screen);
    lv_label_set_text(l1, "GAME OVER");
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_align(l1, LV_ALIGN_CENTER, 0, -20);
    
    lv_obj_t *l2 = lv_label_create(go_screen);
    lv_label_set_text(l2, "Score: 0");
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);
    lv_obj_align(l2, LV_ALIGN_CENTER, 0, 20);
    
    sleep_ms(3000);
}

// ─── Game Cleanup ────────────────────────────────────────────────────────
void mario_game_cleanup(void)
{
    if (mario_state.canvas)
    {
        lv_obj_delete(mario_state.canvas);
    }
    game_state = GAME_STATE_IDLE;
    printf("[MARIO] Game cleaned up\n");
}
