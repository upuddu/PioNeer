#ifndef MARIO_GAME_H
#define MARIO_GAME_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

// ─── Game Constants ───────────────────────────────────────────────────────
#define MARIO_MAP_HEIGHT 9
#define MARIO_MAP_WIDTH 40
#define MARIO_SPRITE_WIDTH 16
#define MARIO_SPRITE_HEIGHT 24
#define MARIO_TILE_SIZE 16
#define MARIO_SCREEN_WIDTH 480
#define MARIO_SCREEN_HEIGHT 320

// ─── Color Definitions for LVGL ──────────────────────────────────────────
#define MARIO_COLOR_BLUE lv_color_hex(0x0337)
#define MARIO_COLOR_SKY lv_color_hex(0x0080FF)
#define MARIO_COLOR_GREEN lv_color_hex(0x00FF00)
#define MARIO_COLOR_BROWN lv_color_hex(0x8B4513)
#define MARIO_COLOR_RED lv_color_hex(0xFF0000)

// Game states
typedef enum {
    GAME_STATE_INIT = 0,
    GAME_STATE_TITLE = 1,
    GAME_STATE_RUNNING = 2,
    GAME_STATE_GAME_OVER = 3,
    GAME_STATE_IDLE = 4
} GameState;

// ─── Game Data Structure ───────────────────────────────────────────────────
typedef struct {
    // Player position
    int pos_x;
    int pos_y;
    int collision_x;
    int collision_y;

    // Physics
    float vel_f_x;
    float vel_f_y;
    int vel_x;
    int vel_y;
    float acc_x;
    float friction;
    float gravity;

    // Collision flags
    int col_bottom;
    int col_top;
    int col_right;
    int col_left;

    // Game state
    int jump_flag;
    int jump_counter;
    int sprite_id;          // 1-6: different Mario states
    int time;
    uint32_t time_sec;
    int score;
    int death;
    bool game_over;

    // Input state (updated by button callback)
    bool btn_jump_pressed;
    bool btn_left_pressed;
    bool btn_right_pressed;
    bool btn_down_pressed;

    // LVGL components
    lv_obj_t *canvas;
} MarioGameState;

// ─── Game Map ───────────────────────────────────────────────────────────
extern const int mario_map[MARIO_MAP_HEIGHT][MARIO_MAP_WIDTH];

// ─── Function Declarations ───────────────────────────────────────────────
// Initialization - call display_init() first, then this
void mario_game_init(void);

// Game loop (call repeatedly from main)
void mario_game_update(void);

// Game state management
GameState mario_get_state(void);
void mario_set_state(GameState state);
MarioGameState* mario_get_state_data(void);

// Input handling (called by button callback)
void mario_button_callback(uint8_t btn, bool pressed);

// Display functions
void mario_display_title(void);
void mario_display_game_over(void);

// Cleanup
void mario_game_cleanup(void);

#endif // MARIO_GAME_H
