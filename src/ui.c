#include "ui.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "sd_api.h"
#include "lvgl/lvgl.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/structs/scb.h"
#include "hardware/watchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Flash Configuration ───────────────────────────────────────────────────────
// We will dedicate the first 1MB of Flash to the PioNeer menu.
// Games will be flashed starting at the 1MB mark (0x100000 offset).
#define GAME_FLASH_OFFSET 0x100000
#define GAME_XIP_BASE     (XIP_BASE + GAME_FLASH_OFFSET)

// ── Game list (dynamically loaded from SD card) ────────────────────────────────
#define MAX_GAMES 256

static char *game_names[MAX_GAMES];
static lv_color_t game_colors[MAX_GAMES];
static int num_games = 0;

// ── State ─────────────────────────────────────────────────────────────────────
static int cursor = 0;         // which tile is highlighted

// Static buffer used to transfer the game from SD to Flash 
// (Avoids heap fragmentation and memory allocation failures)
#define GAME_BUF_SIZE (300 * 1024)
static uint8_t game_transfer_buffer[GAME_BUF_SIZE] __attribute__((aligned(4)));

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
            // NEW: Skip macOS metadata files (files starting with "._")
            if (strncmp(file_list[i].name, "._", 2) == 0) {
                continue;
            }

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
        printf("[UI] ERROR: No game files found (only directories or metadata?)\n");
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
    int cols = (num_games <= 3) ? num_games : 3;
    int rows = (num_games + cols - 1) / cols; 

    // Adjust tile size based on number of rows
    int tile_w = 148;
    int tile_h = 83;
    int x_start = 6;
    int y_start = 44;
    int gap = 6;

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
        lv_obj_set_style_border_opa(tiles[i], LV_OPA_TRANSP, 0); 
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
    lv_obj_set_style_border_opa(tiles[prev], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(tiles[next], LV_OPA_COVER, 0);
    cursor = next;
}

// ── Launch selected game (DYNAMIC RP2350 SCAN LOGIC) ─────────────────────────
static void launch_game(int game)
{
    printf("[MENU] Preparing to flash: %s\n", game_names[game]);
    
    char path[256];
    snprintf(path, sizeof(path), "0:/games/%s", game_names[game]);
    
    // 1. Read binary from SD into static RAM buffer
    size_t bytes_read = 0;
    if (!sd_card_read_file(path, game_transfer_buffer, GAME_BUF_SIZE, &bytes_read) || bytes_read == 0)
    {
        printf("[MENU] ERROR: Failed to load %s from SD card.\n", game_names[game]);
        return;
    }
    
    printf("[MENU] Loaded %u bytes into RAM. Flashing to XIP...\n", (unsigned)bytes_read);
    
    // 2. Erase and Program Flash
    uint32_t ints = save_and_disable_interrupts();
    
    size_t erase_size = ((bytes_read + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    flash_range_erase(GAME_FLASH_OFFSET, erase_size);
    
    size_t prog_size = ((bytes_read + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    flash_range_program(GAME_FLASH_OFFSET, game_transfer_buffer, prog_size);
    
    restore_interrupts(ints);
    
    printf("[MENU] Flash complete. Scanning for RP2350 Vector Table...\n");
    
    // 3. Dynamically locate the Vector Table
    uint32_t *search_ptr = (uint32_t *)GAME_XIP_BASE;
    uint32_t sp = 0, pc = 0;
    bool found_vt = false;
    
    // Scan the first 4KB of the flashed image
    for (int i = 0; i < 1024; i++) {
        uint32_t val_sp = search_ptr[i];
        uint32_t val_pc = search_ptr[i+1];
        
        // A valid Stack Pointer is in SRAM (0x200... range)
        // A valid Program Counter is in Flash (0x10... range)
        if ((val_sp & 0xFFF00000) == 0x20000000 && (val_pc & 0xFF000000) == 0x10000000) {
            // Cortex-M thumb instructions must be odd numbers (bit 0 is 1)
            if (val_pc & 1) { 
                sp = val_sp;
                pc = val_pc;
                found_vt = true;
                printf("[MENU] Found Vector Table at offset 0x%03X\n", i * 4);
                break;
            }
        }
    }
    
    if (!found_vt) {
        printf("[MENU] FATAL: Could not find a valid Vector Table in the binary!\n");
        while(1);
    }

    printf("[MENU] Game SP: 0x%08lX\n", sp);
    printf("[MENU] Game PC: 0x%08lX\n", pc);

    // Sanity check: Did the linker script actually work in the Autobahn project?
    if ((pc & 0xFFF00000) != GAME_XIP_BASE) {
        printf("[MENU] FATAL: Game was compiled for the wrong offset!\n");
        printf("[MENU] PC points to 0x%08lX, but it should start with 0x%03lX\n", pc, (GAME_XIP_BASE >> 20));
        while(1); 
    }

    printf("[MENU] Triggering Watchdog hardware reset...\n");
    
    watchdog_reboot(pc, sp, 10);
    
    while(1);
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
    if (!load_games_from_sd())
    {
        printf("[UI] FATAL: Could not load games from SD card\n");
        return;
    }

    build_menu_screen();
    reset_menu();

    printf("[MENU] Initialized with %d games.\n", num_games);
}

// ── Main loop tick ────────────────────────────────────────────────────────────
void main_menu_run(void)
{
    static uint32_t last_input_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (now - last_input_ms < 200)
    {
        lv_timer_handler();
        return;
    }

    JoystickReading joy = joystick_read();
    int dx = (int)joy.x - 2048;
    int dy = (int)joy.y - 2048;
    int threshold = 800;

    int prev = cursor;
    int next = cursor;
    int cols = (num_games <= 3) ? num_games : 3;

    if (dx > threshold)
        next = cursor + 1; 
    else if (dx < -threshold)
        next = cursor - 1; 
    else if (dy > threshold)
        next = cursor + cols; 
    else if (dy < -threshold)
        next = cursor - cols; 

    if (joystick_sw_consume())
    {
        launch_game(cursor);
        // Note: reset_menu() will no longer execute because launch_game() takes total hardware control.
        reset_menu(); 
        last_input_ms = now;
        return;
    }

    if (next >= 0 && next < num_games && next != prev)
    {
        update_cursor(prev, next);
        last_input_ms = now;
    }

    lv_timer_handler();
}