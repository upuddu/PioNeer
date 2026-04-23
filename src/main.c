#include "pico/stdlib.h"
#include "config.h"
#include "spi_display.h"
#include "lvgl.h"

// LVGL tick interval in milliseconds
#define LV_TICK_PERIOD_MS 5

int main(void) {
    stdio_init_all();
    
    // Initialize LVGL
    lv_init();
    
    // Initialize display driver
    display_init();
    
    // Create a label on the current screen
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    
    lv_obj_t * label1 = lv_label_create(scr);
    lv_label_set_text(label1, "PioNeer!");
    lv_obj_set_style_text_color(label1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, -20);
    
    lv_obj_t * label2 = lv_label_create(scr);
    lv_label_set_text(label2, "SPI OK");
    lv_obj_set_style_text_color(label2, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align(label2, LV_ALIGN_CENTER, 0, 20);
    
    lv_obj_t * label3 = lv_label_create(scr);
    lv_label_set_text(label3, "we made it");
    lv_obj_set_style_text_color(label3, lv_color_hex(0xFF00FF), LV_PART_MAIN);
    lv_obj_align(label3, LV_ALIGN_CENTER, 0, 60);

    // Main loop
    while (true) {
        // Let LVGL do its work
        lv_timer_handler();
        
        // Sleep to save power
        sleep_ms(LV_TICK_PERIOD_MS);
        
        // Tell LVGL how much time has passed
        lv_tick_inc(LV_TICK_PERIOD_MS);
    }
    
    return 0;
}
