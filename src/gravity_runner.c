#include "gravity_runner.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "audio.h"
#include "led_strip.h"
#include "config.h"
#include "lvgl/lvgl.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define NUM_SEGMENTS 12
#define TUNNEL_RADIUS 50.0f
#define FOV 6.0f
#define SEGMENT_SPACING 15.0f

typedef struct {
    float z;
    uint8_t walls[4]; // 0=Bottom, 1=Right, 2=Top, 3=Left
} TunnelSegment;

static TunnelSegment segments[NUM_SEGMENTS];
static float current_speed = 10.0f;
static float distance_traveled = 0.0f;

static float current_rot = 0.0f;
static int target_rot_idx = 0; 

static float player_x_pos = 0.0f; 
static float player_y_pos = 0.0f; 
static float player_vy = 0.0f;

static bool game_over = false;
static int score = 0;

static bool pending_jump = false;
static bool pending_flip = false;
static bool pending_restart = false;

static lv_obj_t *screen;
static lv_obj_t *score_label;
static lv_obj_t *game_over_label;

static lv_obj_t *ring_lines[NUM_SEGMENTS][4];
static lv_obj_t *conn_lines[NUM_SEGMENTS][4];
static lv_point_precise_t ring_points[NUM_SEGMENTS][4][2];
static lv_point_precise_t conn_points[NUM_SEGMENTS][4][2];

static lv_obj_t *alien_body;
static lv_style_t style_line;

static void generate_segment(int idx, float start_z) {
    segments[idx].z = start_z;
    for (int i=0; i<4; i++) segments[idx].walls[i] = 1;
    
    if (distance_traveled > 50.0f && start_z > 10.0f) {
        if (rand() % 100 < 50) {
            int hole_wall = rand() % 4;
            segments[idx].walls[hole_wall] = 0;
            if (rand() % 100 < 20) {
                segments[idx].walls[(hole_wall + 1) % 4] = 0;
            }
        }
    }
}

void gravity_runner_init(void) {
    printf("[GRAVITY RUNNER] Init\n");
    game_over = false;
    score = 0;
    current_speed = 25.0f;
    distance_traveled = 0.0f;
    current_rot = 0.0f;
    target_rot_idx = 0;
    player_x_pos = 0.0f;
    player_y_pos = 0.0f;
    player_vy = 0.0f;

    screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x050510), 0); // dark blue/black space
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);
    lv_style_set_line_color(&style_line, lv_color_hex(0x00FFFF)); // Cyan

    for (int i = 0; i < NUM_SEGMENTS; i++) {
        for (int j = 0; j < 4; j++) {
            ring_lines[i][j] = lv_line_create(screen);
            lv_obj_add_style(ring_lines[i][j], &style_line, 0);
            conn_lines[i][j] = lv_line_create(screen);
            lv_obj_add_style(conn_lines[i][j], &style_line, 0);
        }
        generate_segment(i, 2.0f + i * SEGMENT_SPACING);
    }

    alien_body = lv_obj_create(screen);
    lv_obj_set_size(alien_body, 20, 20);
    lv_obj_set_style_radius(alien_body, 10, 0);
    lv_obj_set_style_bg_color(alien_body, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_border_color(alien_body, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(alien_body, 2, 0);
    lv_obj_remove_flag(alien_body, LV_OBJ_FLAG_SCROLLABLE);

    score_label = lv_label_create(screen);
    lv_label_set_text(score_label, "Score: 0");
    lv_obj_set_style_text_color(score_label, lv_color_white(), 0);
    lv_obj_set_pos(score_label, 10, 10);

    game_over_label = lv_label_create(screen);
    lv_label_set_text(game_over_label, "GAME OVER\nPress Button to Restart");
    lv_obj_set_style_text_color(game_over_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(game_over_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(game_over_label);
    lv_obj_add_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);

    led_strip_clear();
    led_strip_show();
}

static void draw_tunnel(void) {
    float target_angle = target_rot_idx * 90.0f;
    float diff = target_angle - current_rot;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    current_rot += diff * 0.15f; 

    float angle_rad = current_rot * M_PI / 180.0f;
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    float px[NUM_SEGMENTS][4];
    float py[NUM_SEGMENTS][4];
    
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        float z = segments[i].z;
        if (z < 0.1f) z = 0.1f;
        float scale = FOV / z;
        
        for (int j = 0; j < 4; j++) {
            float orig_x = TUNNEL_RADIUS * ((j == 0 || j == 3) ? -1 : 1);
            float orig_y = TUNNEL_RADIUS * ((j == 0 || j == 1) ? 1 : -1);
            
            float rx = orig_x * cos_a - orig_y * sin_a;
            float ry = orig_x * sin_a + orig_y * cos_a;
            
            px[i][j] = 240 + rx * scale;
            py[i][j] = 160 + ry * scale;
        }
    }
    
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        for (int j = 0; j < 4; j++) {
            if (segments[i].walls[j]) {
                ring_points[i][j][0].x = px[i][j];
                ring_points[i][j][0].y = py[i][j];
                int next_j = (j + 1) % 4;
                ring_points[i][j][1].x = px[i][next_j];
                ring_points[i][j][1].y = py[i][next_j];
                lv_line_set_points(ring_lines[i][j], ring_points[i][j], 2);
                lv_obj_remove_flag(ring_lines[i][j], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(ring_lines[i][j], LV_OBJ_FLAG_HIDDEN);
            }
            
            // Find next segment index (sorted by generation logic)
            // The segment in front is just the one generated after it?
            // Actually, we can just connect to (i+1)%NUM_SEGMENTS if it has greater z.
            int next_i = (i + 1) % NUM_SEGMENTS;
            if (segments[next_i].z > segments[i].z) {
                conn_points[i][j][0].x = px[i][j];
                conn_points[i][j][0].y = py[i][j];
                conn_points[i][j][1].x = px[next_i][j];
                conn_points[i][j][1].y = py[next_i][j];
                lv_line_set_points(conn_lines[i][j], conn_points[i][j], 2);
                lv_obj_remove_flag(conn_lines[i][j], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(conn_lines[i][j], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void gravity_runner_update(void) {
    if (pending_restart) {
        pending_restart = false;
        gravity_runner_init();
        return;
    }

    if (game_over) {
        pending_jump = false;
        pending_flip = false;
        return;
    }

    if (pending_flip) {
        pending_flip = false;
        target_rot_idx += 2; 
        player_x_pos = -player_x_pos; 
    }
    if (pending_jump) {
        pending_jump = false;
        if (player_y_pos <= 0.1f) {
            player_vy = 12.0f;
        }
    }

    float dt = 0.05f;  
    distance_traveled += current_speed * dt;
    current_speed += 0.005f; // accelerate slowly

    JoystickReading joy = joystick_read();
    float dx = ((int)joy.x - 2048) / 2048.0f;
    if (fabs(dx) < 0.2f) dx = 0;
    
    player_x_pos += dx * 0.15f;
    
    // Wall transition
    if (player_x_pos > 1.0f) {
        player_x_pos = -1.0f; 
        target_rot_idx++; 
    } else if (player_x_pos < -1.0f) {
        player_x_pos = 1.0f; 
        target_rot_idx--; 
    }

    // Jump physics
    player_y_pos += player_vy;
    player_vy -= 1.5f; 
    if (player_y_pos < 0.0f) {
        player_y_pos = 0.0f;
        player_vy = 0.0f;
    }

    for (int i=0; i<NUM_SEGMENTS; i++) {
        float old_z = segments[i].z;
        segments[i].z -= current_speed * dt;
        
        if (segments[i].z < 2.0f && old_z >= 2.0f) {
            int bottom_wall = (target_rot_idx % 4);
            if (bottom_wall < 0) bottom_wall += 4;
            
            if (segments[i].walls[bottom_wall] == 0 && player_y_pos <= 10.0f) {
                // Fell in hole
                game_over = true;
                lv_obj_remove_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);
                led_strip_set_all_color(COLOR_RED);
                led_strip_show();
            } else {
                score += 10;
                char buf[32];
                snprintf(buf, sizeof(buf), "Score: %d", score);
                lv_label_set_text(score_label, buf);
                
                // Light up random LED for effect
                led_strip_clear();
                led_strip_set_pixel_color(rand() % 10, COLOR_CYAN);
                led_strip_show();
            }
        }
        
        if (segments[i].z < 0.0f) {
            float max_z = 0;
            for (int j=0; j<NUM_SEGMENTS; j++) {
                if (segments[j].z > max_z) max_z = segments[j].z;
            }
            generate_segment(i, max_z + SEGMENT_SPACING);
        }
    }

    draw_tunnel();

    // Update alien position
    float p_scale = FOV / 2.0f;
    float p_orig_x = player_x_pos * TUNNEL_RADIUS;
    float p_orig_y = TUNNEL_RADIUS - player_y_pos;
    float screen_x = 240 + p_orig_x * p_scale;
    float screen_y = 160 + p_orig_y * p_scale;
    
    // Scale alien slightly based on jump
    int size = 20 + (int)(player_y_pos * 0.2f);
    lv_obj_set_size(alien_body, size, size);
    lv_obj_set_pos(alien_body, screen_x - size/2, screen_y - size/2);
}

void gravity_runner_button_callback(uint8_t btn, bool pressed) {
    if (!pressed) return;
    
    if (game_over) {
        pending_restart = true;
        return;
    }

    if (btn == BTN_A) {
        pending_flip = true;
    } else if (btn == BTN_B || btn == BTN_X || btn == BTN_Y) {
        pending_jump = true;
    }
}
