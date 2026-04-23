#include "tictactoe.h"
#include "lvgl.h"
#include "config.h"
#include "adc_joystick.h"
#include "audio.h"
#include "sounds.h"
#include "led_strip.h"
#include "spi_display.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "lvgl/src/drivers/display/st7796/lv_st7796.h"
#include "gpio_buttons.h"
#include <stdio.h>
#include <stdlib.h>

// ── Game State ────────────────────────────────────────────────────────────────
typedef enum { EMPTY, PLAYER_O, PLAYER_X } CellValue;
static CellValue board[3][3];
static int cursor_r = 1, cursor_c = 1;
static bool player_o_turn = true;
static bool game_over = false;

// When a line wins, these coords point at the three cells forming the line.
static int win_cells[3][2];
static bool has_win_line = false;

// ── UI Elements ───────────────────────────────────────────────────────────────
static lv_obj_t *cells[3][3];
static lv_obj_t *status_label;
static lv_obj_t *title_label;
static lv_obj_t *overlay = NULL;       // dim background for end-of-game banner
static lv_obj_t *banner_label = NULL;  // big "O WINS!" / "X WINS!" / "DRAW"
static uint32_t last_move_time = 0;
static uint32_t win_flash_t0 = 0;

// ── LED-strip event timestamps (ms since boot) ────────────────────────────────
// When these are 0 the effect is inactive. When set, led_tick() applies the
// corresponding transient effect for a short window after the timestamp.
static uint32_t led_move_t0  = 0;
static uint32_t led_place_t0 = 0;
static int led_place_r = 0, led_place_c = 0;    // which cell was just played
static CellValue led_place_player = EMPTY;
static uint32_t led_last_tick = 0;              // throttle for led_strip_show()
static CellValue winner_player = EMPTY;         // who won (for LED colours)

// ── Display Callbacks (Re-implemented because old files cannot be modified) ──
static void local_send_cmd(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size, const uint8_t *param, size_t param_size) {
    gpio_put(SPI_DC_PIN, 0);
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_size);
    if (param_size > 0) {
        gpio_put(SPI_DC_PIN, 1);
        spi_write_blocking(SPI_PORT, param, param_size);
    }
    gpio_put(SPI_CS_PIN, 1);
}

static void local_send_color(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size, uint8_t *param, size_t param_size) {
    gpio_put(SPI_DC_PIN, 0);
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_size);
    gpio_put(SPI_DC_PIN, 1);
    for (uint32_t i = 0; i < param_size; i += 2) {
        uint8_t temp = param[i];
        param[i] = param[i + 1];
        param[i + 1] = temp;
    }
    spi_write_blocking(SPI_PORT, param, param_size);
    gpio_put(SPI_CS_PIN, 1);
    lv_display_flush_ready(disp);
}

static uint32_t local_tick_cb(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

// ── Internal Logic ────────────────────────────────────────────────────────────

static bool cell_is_winning(int r, int c) {
    if (!has_win_line) return false;
    for (int i = 0; i < 3; i++) {
        if (win_cells[i][0] == r && win_cells[i][1] == c) return true;
    }
    return false;
}

static void update_visuals(void) {
    // Pulse hue for the winning line: ms-based sinusoidal brightness.
    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool phase_bright = ((now - win_flash_t0) / 180) & 1;
    uint32_t win_bg   = phase_bright ? 0x27AE60 : 0x145A32; // green pulse
    uint32_t win_brdr = phase_bright ? 0x2ECC71 : 0x1E8449;

    for(int r=0; r<3; r++) {
        for(int c=0; c<3; c++) {
            lv_obj_t *lbl = lv_obj_get_child(cells[r][c], 0);
            if (board[r][c] == PLAYER_O) {
                lv_label_set_text(lbl, "O");
                lv_obj_set_style_text_color(lbl, lv_color_hex(0x3498DB), 0);
            } else if (board[r][c] == PLAYER_X) {
                lv_label_set_text(lbl, "X");
                lv_obj_set_style_text_color(lbl, lv_color_hex(0xE74C3C), 0);
            } else {
                lv_label_set_text(lbl, "");
            }

            bool winner = cell_is_winning(r, c);

            if (winner) {
                lv_obj_set_style_border_color(cells[r][c], lv_color_hex(win_brdr), 0);
                lv_obj_set_style_border_width(cells[r][c], 4, 0);
                lv_obj_set_style_bg_color(cells[r][c], lv_color_hex(win_bg), 0);
            } else if (r == cursor_r && c == cursor_c && !game_over) {
                lv_obj_set_style_border_color(cells[r][c], lv_color_hex(0xF1C40F), 0);
                lv_obj_set_style_border_width(cells[r][c], 3, 0);
                lv_obj_set_style_bg_color(cells[r][c], lv_color_hex(0x2C2C2C), 0);
            } else {
                lv_obj_set_style_border_color(cells[r][c], lv_color_hex(0x333333), 0);
                lv_obj_set_style_border_width(cells[r][c], 2, 0);
                lv_obj_set_style_bg_color(cells[r][c], lv_color_hex(0x1E1E1E), 0);
            }
        }
    }
    if (!game_over) {
        lv_label_set_text(status_label, player_o_turn ? "Turn: O" : "Turn: X");
        lv_obj_set_style_text_color(status_label,
            lv_color_hex(player_o_turn ? 0x3498DB : 0xE74C3C), 0);
    }
}

// Returns true and fills win_cells[] if player p has three in a row.
static bool check_win(CellValue p) {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] == p && board[i][1] == p && board[i][2] == p) {
            win_cells[0][0]=i; win_cells[0][1]=0;
            win_cells[1][0]=i; win_cells[1][1]=1;
            win_cells[2][0]=i; win_cells[2][1]=2;
            return true;
        }
        if (board[0][i] == p && board[1][i] == p && board[2][i] == p) {
            win_cells[0][0]=0; win_cells[0][1]=i;
            win_cells[1][0]=1; win_cells[1][1]=i;
            win_cells[2][0]=2; win_cells[2][1]=i;
            return true;
        }
    }
    if (board[0][0] == p && board[1][1] == p && board[2][2] == p) {
        win_cells[0][0]=0; win_cells[0][1]=0;
        win_cells[1][0]=1; win_cells[1][1]=1;
        win_cells[2][0]=2; win_cells[2][1]=2;
        return true;
    }
    if (board[0][2] == p && board[1][1] == p && board[2][0] == p) {
        win_cells[0][0]=0; win_cells[0][1]=2;
        win_cells[1][0]=1; win_cells[1][1]=1;
        win_cells[2][0]=2; win_cells[2][1]=0;
        return true;
    }
    return false;
}

static bool check_draw(void) {
    for(int r=0; r<3; r++) for(int c=0; c<3; c++) if(board[r][c] == EMPTY) return false;
    return true;
}

// ── LED strip helpers ─────────────────────────────────────────────────────────
// 10-LED strip: LEDs 0..8 mirror the 3x3 board (row-major, so LED index = r*3+c),
// LED 9 is the turn / game-status indicator. We refresh at ~33 Hz.

// Simple triangle-wave breathing: returns 0..peak over duration_ms.
static uint8_t breathe(uint32_t now, uint32_t period_ms, uint8_t peak) {
    uint32_t p = now % period_ms;
    uint32_t half = period_ms / 2;
    uint32_t v = (p < half) ? p : (period_ms - p);
    return (uint8_t)((v * peak) / half);
}

// Set one LED using a player colour with optional brightness override.
static void led_set_player(uint8_t idx, CellValue v, uint8_t bright) {
    uint8_t r=0, g=0, b=0;
    if (v == PLAYER_O)       { r = 52;  g = 152; b = 219; } // blue
    else if (v == PLAYER_X)  { r = 231; g =  76; b =  60; } // red
    // Scale by brightness.
    r = (uint16_t)r * bright / 255;
    g = (uint16_t)g * bright / 255;
    b = (uint16_t)b * bright / 255;
    led_strip_set_pixel(idx, r, g, b);
}

static void led_tick(uint32_t now) {
    // Throttle LED refreshes — WS2812 updates are slow-ish.
    if (now - led_last_tick < 30) return;
    led_last_tick = now;

    // Clear frame first.
    led_strip_clear();

    if (game_over && has_win_line) {
        // Win celebration: winning cells strobe in winner's colour, others dim.
        bool strobe = ((now / 120) & 1);
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                int idx = r * 3 + c;
                bool is_win = false;
                for (int k = 0; k < 3; k++) {
                    if (win_cells[k][0] == r && win_cells[k][1] == c) { is_win = true; break; }
                }
                if (is_win) {
                    led_set_player(idx, winner_player, strobe ? 255 : 90);
                } else if (board[r][c] != EMPTY) {
                    led_set_player(idx, board[r][c], 25);
                }
            }
        }
        // Turn LED: winner colour, strong.
        led_set_player(9, winner_player, strobe ? 255 : 120);
        led_strip_show();
        return;
    }

    if (game_over && !has_win_line) {
        // Draw: slow yellow pulse on every pixel.
        uint8_t v = breathe(now, 900, 180);
        for (int i = 0; i < 10; i++) {
            led_strip_set_pixel(i, v, v, 0); // yellow
        }
        led_strip_show();
        return;
    }

    // ── In-game: base layer = board state ──
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int idx = r * 3 + c;
            if (board[r][c] != EMPTY) {
                led_set_player(idx, board[r][c], 180);
            } else {
                // Empty cell: faint indicator of whose turn it is.
                uint8_t bright = 12;
                led_set_player(idx, player_o_turn ? PLAYER_O : PLAYER_X, bright);
            }
        }
    }

    // Cursor overlay: pulse the cursor cell in gold.
    {
        int cidx = cursor_r * 3 + cursor_c;
        uint8_t b = 120 + breathe(now, 800, 135); // 120..255 gold pulse
        led_strip_set_pixel(cidx, b, (uint8_t)(b * 3 / 4), 0);
    }

    // Transient: just placed → bright flash on that cell for 220 ms.
    if (led_place_t0 && now - led_place_t0 < 220) {
        uint32_t age = now - led_place_t0;
        uint8_t bright = (uint8_t)(255 - (age * 255 / 220));
        led_set_player(led_place_r * 3 + led_place_c, led_place_player, bright);
    }

    // Transient: cursor moved → quick white highlight on cursor for 120 ms.
    if (led_move_t0 && now - led_move_t0 < 120) {
        uint32_t age = now - led_move_t0;
        uint8_t bright = (uint8_t)(255 - (age * 255 / 120));
        int cidx = cursor_r * 3 + cursor_c;
        uint8_t w = (uint8_t)(bright / 3);
        led_strip_set_pixel(cidx, w, w, w);
    }

    // Turn indicator LED (index 9).
    {
        uint8_t b = 60 + breathe(now, 1400, 90);
        led_set_player(9, player_o_turn ? PLAYER_O : PLAYER_X, b);
    }

    led_strip_show();
}

// Show a large centered banner with a dim backdrop over the grid.
static void show_end_banner(const char *text, uint32_t color) {
    lv_obj_t *scr = lv_screen_active();

    if (!overlay) {
        overlay = lv_obj_create(scr);
        lv_obj_set_size(overlay, 360, 120);
        lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(overlay, 16, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
        lv_obj_set_style_border_width(overlay, 3, 0);
        lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

        banner_label = lv_label_create(overlay);
        lv_obj_center(banner_label);
        lv_obj_set_style_text_font(banner_label, &lv_font_montserrat_40, 0);
    }
    lv_obj_set_style_border_color(overlay, lv_color_hex(color), 0);
    lv_obj_set_style_text_color(banner_label, lv_color_hex(color), 0);
    lv_label_set_text(banner_label, text);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay);
}

// ── Public API ────────────────────────────────────────────────────────────────

void tictactoe_init(void) {
    // 1. Hardware Setup (Standalone)
    spi_init(SPI_PORT, SPI_BAUD_HZ);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SPI_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);

    int pins[] = {SPI_CS_PIN, SPI_DC_PIN, SPI_RST_PIN, SPI_BL_PIN};
    for(int i=0; i<4; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 1);
    }

    gpio_put(SPI_RST_PIN, 0); sleep_ms(100); gpio_put(SPI_RST_PIN, 1); sleep_ms(100);

    lv_init();
    lv_tick_set_cb(local_tick_cb);
    lv_display_t *disp = lv_st7796_create(LCD_HEIGHT, LCD_WIDTH, LV_LCD_FLAG_NONE, local_send_cmd, local_send_color);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    uint32_t buf_size = LCD_WIDTH * LCD_HEIGHT * 2 / 10;
    lv_display_set_buffers(disp, malloc(buf_size), malloc(buf_size), buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    buttons_init(); // Sets up shared IRQ callback
    joystick_init();
    joystick_sw_init();
    init_pwm_audio();
    init_wavetable();
    led_strip_init();
    led_strip_set_brightness(80); // comfortable indoor brightness
    led_strip_clear();
    led_strip_show();

    // 2. Game State Reset
    for(int r=0; r<3; r++) for(int c=0; c<3; c++) board[r][c] = EMPTY;
    game_over = false;
    has_win_line = false;
    player_o_turn = true;
    cursor_r = 1; cursor_c = 1;
    overlay = NULL;
    banner_label = NULL;
    led_move_t0 = 0;
    led_place_t0 = 0;
    led_last_tick = 0;
    winner_player = EMPTY;

    // 3. UI Construction — rotated screen is 480×320 (landscape).
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F0F0F), 0);

    title_label = lv_label_create(scr);
    lv_label_set_text(title_label, "Tic Tac Toe");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    status_label = lv_label_create(scr);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ── Grid: scaled down so it no longer covers the title. ──
    // Cells 60×60 on a 68px pitch → grid inner footprint 196×196.
    // Outer container 210×210 (accounting for LVGL default border/padding).
    const int CELL = 60;
    const int PITCH = 68;
    const int GRID = 210;

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, GRID, GRID);
    // Nudge down a bit so it sits between title (top ~50px) and status (bot ~40px).
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    for(int r=0; r<3; r++) {
        for(int c=0; c<3; c++) {
            cells[r][c] = lv_obj_create(grid);
            lv_obj_set_size(cells[r][c], CELL, CELL);
            lv_obj_set_pos(cells[r][c], c * PITCH, r * PITCH);
            lv_obj_set_style_radius(cells[r][c], 8, 0);
            lv_obj_set_style_pad_all(cells[r][c], 0, 0);
            lv_obj_remove_flag(cells[r][c], LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *lbl = lv_label_create(cells[r][c]);
            lv_label_set_text(lbl, "");
            lv_obj_center(lbl);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_32, 0);
        }
    }
    update_visuals();
}

void tictactoe_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    lv_timer_handler();

    if (game_over) {
        // Keep flashing the winning line while waiting for a restart click.
        if (has_win_line) update_visuals();
        led_tick(now);
        if (joystick_sw_consume()) tictactoe_init();
        return;
    }

    if (now - last_move_time > 200) {
        JoystickReading joy = joystick_read();
        int dx = (int)joy.x - 2048;
        int dy = (int)joy.y - 2048;
        bool moved = false;

        if (dx > 1000) { cursor_c = (cursor_c + 1) % 3; moved = true; }
        else if (dx < -1000) { cursor_c = (cursor_c + 2) % 3; moved = true; }
        if (dy > 1000) { cursor_r = (cursor_r + 1) % 3; moved = true; }
        else if (dy < -1000) { cursor_r = (cursor_r + 2) % 3; moved = true; }

        if (moved) {
            last_move_time = now;
            led_move_t0 = now;
            play_sample(SND_MOVE, SND_MOVE_LEN, SND_SAMPLE_RATE);
            update_visuals();
        }
    }

    if (joystick_sw_consume()) {
        if (board[cursor_r][cursor_c] == EMPTY) {
            CellValue placed = player_o_turn ? PLAYER_O : PLAYER_X;
            board[cursor_r][cursor_c] = placed;
            play_sample(SND_PLACE, SND_PLACE_LEN, SND_SAMPLE_RATE);
            led_place_t0 = now;
            led_place_r = cursor_r;
            led_place_c = cursor_c;
            led_place_player = placed;

            if (check_win(placed)) {
                game_over = true;
                has_win_line = true;
                winner_player = placed;
                win_flash_t0 = now;
                lv_label_set_text(status_label, "Press joystick to restart");
                lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
                show_end_banner(placed == PLAYER_O ? "O WINS!" : "X WINS!",
                                placed == PLAYER_O ? 0x3498DB : 0xE74C3C);
                play_sample(SND_WIN, SND_WIN_LEN, SND_SAMPLE_RATE);
            } else if (check_draw()) {
                game_over = true;
                has_win_line = false;
                lv_label_set_text(status_label, "Press joystick to restart");
                lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
                show_end_banner("DRAW", 0xF1C40F);
                play_sample(SND_LOSE, SND_LOSE_LEN, SND_SAMPLE_RATE);
            } else {
                player_o_turn = !player_o_turn;
            }
            update_visuals();
        }
    }

    led_tick(now);
}
