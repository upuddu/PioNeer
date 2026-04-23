#include "ui.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "lvgl.h"
#include "pico/stdlib.h"
#include <stdio.h>

// ── Game list ─────────────────────────────────────────────────────────────────
#define NUM_GAMES 9

static const char *game_names[NUM_GAMES] = {
    "Game 1", "Game 2", "Game 3",
    "Game 4", "Game 5", "Game 6",
    "Game 7", "Game 8", "Game 9"};

static const lv_color_t game_colors[NUM_GAMES] = {
    {.red = 220, .green = 50, .blue = 50},   // red
    {.red = 50, .green = 180, .blue = 50},   // green
    {.red = 50, .green = 50, .blue = 220},   // blue
    {.red = 220, .green = 150, .blue = 50},  // orange
    {.red = 150, .green = 50, .blue = 220},  // purple
    {.red = 50, .green = 200, .blue = 200},  // cyan
    {.red = 220, .green = 50, .blue = 150},  // pink
    {.red = 200, .green = 200, .blue = 50},  // yellow
    {.red = 100, .green = 180, .blue = 100}, // mint
};

// ── State ─────────────────────────────────────────────────────────────────────
static int selected_game = -1; // -1 = in menu
static int cursor = 0;         // 0-8, which tile is highlighted

// ── LVGL objects ─────────────────────────────────────────────────────────────
static lv_obj_t *menu_screen = NULL;
static lv_obj_t *select_screen = NULL;
static lv_obj_t *select_label = NULL;
static lv_obj_t *tiles[NUM_GAMES];
static lv_obj_t *tile_labels[NUM_GAMES];

// ── Build menu screen ─────────────────────────────────────────────────────────
static void build_menu_screen(void)
{
    menu_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(menu_screen, lv_color_black(), 0);

    // Title
    lv_obj_t *title = lv_label_create(menu_screen);
    lv_label_set_text(title, "PioNeer");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // 3x3 grid of tiles
    // 480 wide, 320 tall, title takes ~40px
    // each tile: ~150x85
    int tile_w = 148;
    int tile_h = 83;
    int x_start = 6;
    int y_start = 44;
    int gap = 6;

    for (int i = 0; i < NUM_GAMES; i++)
    {
        int row = i / 3;
        int col = i % 3;
        int x = x_start + col * (tile_w + gap);
        int y = y_start + row * (tile_h + gap);

        tiles[i] = lv_obj_create(menu_screen);
        lv_obj_set_pos(tiles[i], x, y);
        lv_obj_set_size(tiles[i], tile_w, tile_h);
        lv_obj_set_style_bg_color(tiles[i], game_colors[i], 0);
        lv_obj_set_style_border_width(tiles[i], 3, 0);
        lv_obj_set_style_border_color(tiles[i], lv_color_white(), 0);
        lv_obj_set_style_border_opa(tiles[i], LV_OPA_TRANSP, 0); // hidden by default
        lv_obj_set_style_radius(tiles[i], 8, 0);

        tile_labels[i] = lv_label_create(tiles[i]);
        lv_label_set_text(tile_labels[i], game_names[i]);
        lv_obj_set_style_text_color(tile_labels[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(tile_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_center(tile_labels[i]);
    }

    // Build select screen (reused for all selections)
    select_screen = lv_obj_create(NULL);
    select_label = lv_label_create(select_screen);
    lv_obj_set_style_text_font(select_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(select_label, lv_color_white(), 0);
    lv_obj_center(select_label);
}

// ── Highlight cursor tile ─────────────────────────────────────────────────────
static void update_cursor(int prev, int next)
{
    // Remove highlight from previous
    lv_obj_set_style_border_opa(tiles[prev], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(tiles[next], LV_OPA_COVER, 0);
    cursor = next;
}

// ── Show selected game fullscreen ─────────────────────────────────────────────
static void show_selected(int game)
{
    selected_game = game;
    lv_obj_set_style_bg_color(select_screen, game_colors[game], 0);

    char buf[32];
    snprintf(buf, sizeof(buf), "Chosen:\n%s", game_names[game]);
    lv_label_set_text(select_label, buf);
    lv_obj_center(select_label);

    lv_screen_load(select_screen);
    printf("[MENU] Selected: %s\n", game_names[game]);
}

// ── Back to menu ──────────────────────────────────────────────────────────────
static void show_menu(void)
{
    selected_game = -1;
    lv_screen_load(menu_screen);
}

// ── Public init ───────────────────────────────────────────────────────────────
void main_menu_init(void)
{
    build_menu_screen();

    // Show menu, highlight first tile
    lv_screen_load(menu_screen);
    lv_obj_set_style_border_opa(tiles[0], LV_OPA_COVER, 0);

    printf("[MENU] Initialized.\n");
}

// ── Main loop tick ────────────────────────────────────────────────────────────
void main_menu_run(void)
{
    static uint32_t last_input_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Debounce inputs at 200ms
    if (now - last_input_ms < 200)
    {
        lv_timer_handler();
        return;
    }

    JoystickReading joy = joystick_read();
    int dx = (int)joy.x - 2048;
    int dy = (int)joy.y - 2048;
    int threshold = 800;

    // ── In menu: navigate ────────────────────────────────────────────────────
    if (selected_game == -1)
    {
        int prev = cursor;
        int next = cursor;

        if (dx > threshold)
            next = cursor + 1; // right
        else if (dx < -threshold)
            next = cursor - 1; // left
        else if (dy > threshold)
            next = cursor + 3; // down
        else if (dy < -threshold)
            next = cursor - 3; // up

        // A button or joystick click = select
        if (joystick_sw_consume())
        {
            show_selected(cursor);
            last_input_ms = now;
            return;
        }

        // B button = wrap or ignore (nothing to go back to from menu)

        // Clamp next to valid range
        if (next >= 0 && next < NUM_GAMES && next != prev)
        {
            update_cursor(prev, next);
            last_input_ms = now;
        }

        // ── In game screen: B = back to menu ────────────────────────────────────
    }
    else
    {
        if (button_get_state(BTN_B) == BTN_STATE_PRESSED)
        {
            show_menu();
            last_input_ms = now;
        }
    }

    lv_timer_handler();
}