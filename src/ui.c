#include "ui.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "sd_api.h"
#include "lvgl/lvgl.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Game list (dynamically loaded from SD card) ────────────────────────────────
#define MAX_GAMES 256

static char *game_names[MAX_GAMES];
static lv_color_t game_colors[MAX_GAMES];
static int num_games = 0;

// ── State ─────────────────────────────────────────────────────────────────────
static int cursor = 0;         // which tile is highlighted

// ── LVGL objects ─────────────────────────────────────────────────────────────
static lv_obj_t *menu_screen = NULL;
static lv_obj_t *tiles[MAX_GAMES];
static lv_obj_t *tile_labels[MAX_GAMES];

// ── Generate colors for games ─────────────────────────────────────────────────
static lv_color_t generate_color(int index)
{
    static const lv_color_t base_colors[] = {
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
    static const int num_colors = sizeof(base_colors) / sizeof(base_colors[0]);
    return base_colors[index % num_colors];
}

// ── Load games from SD card ───────────────────────────────────────────────────
static bool load_games_from_sd(void)
{
    sd_file_info_t *file_list = NULL;
    size_t file_count = 0;

    printf("[UI] Loading games from SD card...\n");

    // Initialize SD card if not already done
    if (!sd_card_init())
    {
        printf("[UI] ERROR: Failed to initialize SD card\n");
        return false;
    }

    // Check if games directory exists
    if (!sd_card_exists("0:/games"))
    {
        printf("[UI] ERROR: games/ directory not found on SD card\n");
        return false;
    }

    // Read directory contents
    if (!sd_card_get_dir_contents("0:/games", &file_list, &file_count))
    {
        printf("[UI] ERROR: Failed to read games directory\n");
        return false;
    }

    if (file_count == 0)
    {
        printf("[UI] ERROR: No games found in games/ directory\n");
        sd_card_free_dir_contents(file_list);
        return false;
    }

    // Filter out directories, keep only files
    int game_idx = 0;
    for (size_t i = 0; i < file_count && game_idx < MAX_GAMES; i++)
    {
        if (!file_list[i].is_dir)
        {
            // Allocate memory for game name
            size_t name_len = strlen(file_list[i].name) + 1;
            game_names[game_idx] = (char *)malloc(name_len);
            if (game_names[game_idx] == NULL)
            {
                printf("[UI] ERROR: Memory allocation failed\n");
                break;
            }
            strcpy(game_names[game_idx], file_list[i].name);
            game_colors[game_idx] = generate_color(game_idx);
            game_idx++;
        }
    }

    num_games = game_idx;
    sd_card_free_dir_contents(file_list);

    if (num_games == 0)
    {
        printf("[UI] ERROR: No game files found (only directories?)\n");
        return false;
    }

    printf("[UI] Loaded %d games from SD card\n", num_games);
    for (int i = 0; i < num_games; i++)
    {
        printf("  [%d] %s\n", i, game_names[i]);
    }

    return true;
}

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

    // Calculate grid dimensions based on game count
    // Start with 3 columns, calculate rows needed
    int cols = (num_games <= 3) ? num_games : 3;
    int rows = (num_games + cols - 1) / cols; // ceiling division

    // Adjust tile size based on number of rows
    int tile_w = 148;
    int tile_h = 83;
    int x_start = 6;
    int y_start = 44;
    int gap = 6;

    // If we have many rows, shrink tiles
    if (rows > 3)
    {
        tile_h = 60;
        gap = 4;
    }

    for (int i = 0; i < num_games; i++)
    {
        int row = i / cols;
        int col = i % cols;
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
}

// ── Highlight cursor tile ─────────────────────────────────────────────────────
static void update_cursor(int prev, int next)
{
    // Remove highlight from previous
    lv_obj_set_style_border_opa(tiles[prev], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(tiles[next], LV_OPA_COVER, 0);
    cursor = next;
}

// ── Launch selected game ─────────────────────────────────────────────────────
static void launch_game(int game)
{
    printf("[MENU] Loading: %s\n", game_names[game]);
    
    // Construct full path
    char path[256];
    snprintf(path, sizeof(path), "0:/games/%s", game_names[game]);
    
    // Allocate buffer for game binary
    uint8_t *game_buffer = (uint8_t *)malloc(512 * 1024);  // 512KB buffer
    if (game_buffer == NULL)
    {
        printf("[MENU] ERROR: Memory allocation failed\n");
        return;
    }
    
    // Read game binary from SD card
    size_t bytes_read = 0;
    if (!sd_card_read_file(path, game_buffer, 512 * 1024, &bytes_read))
    {
        printf("[MENU] ERROR: Failed to load %s\n", game_names[game]);
        free(game_buffer);
        return;
    }
    
    printf("[MENU] Loaded %u bytes, executing...\n", (unsigned)bytes_read);
    
    // Cast buffer to function pointer and call
    typedef void (*GameFunc)(void);
    GameFunc game_entry = (GameFunc)game_buffer;
    
    // Execute the game
    game_entry();
    
    printf("[MENU] Game returned, back to menu\n");
    
    // Free game memory
    free(game_buffer);
}

// ── Reset menu to first game ──────────────────────────────────────────────────
static void reset_menu(void)
{
    cursor = 0;
    lv_obj_set_style_border_opa(tiles[0], LV_OPA_COVER, 0);
    lv_screen_load(menu_screen);
}

// ── Public init ───────────────────────────────────────────────────────────────
void main_menu_init(void)
{
    // Load games from SD card
    if (!load_games_from_sd())
    {
        printf("[UI] FATAL: Could not load games from SD card\n");
        return;
    }

    build_menu_screen();

    // Reset to first game
    reset_menu();

    printf("[MENU] Initialized with %d games.\n", num_games);
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

    // Navigate menu
    int prev = cursor;
    int next = cursor;

    // Calculate grid dimensions
    int cols = (num_games <= 3) ? num_games : 3;

    if (dx > threshold)
        next = cursor + 1; // right
    else if (dx < -threshold)
        next = cursor - 1; // left
    else if (dy > threshold)
        next = cursor + cols; // down
    else if (dy < -threshold)
        next = cursor - cols; // up

    // A button or joystick click = launch game
    if (joystick_sw_consume())
    {
        launch_game(cursor);
        reset_menu();
        last_input_ms = now;
        return;
    }

    // Clamp next to valid range
    if (next >= 0 && next < num_games && next != prev)
    {
        update_cursor(prev, next);
        last_input_ms = now;
    }

    lv_timer_handler();
}