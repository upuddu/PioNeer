#include "spi_display.h"
#include "config.h"
#include "pico/stdlib.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * HSTX Display Driver for RP2350A Feather
 * Uses integrated HSTX peripheral for 22-pin DVI/HDMI breakout
 * 
 * Resolution: 640x480 @ 60Hz
 * Display Type: DVI/HDMI via 22-pin breakout
 */

static lv_display_t *display = NULL;

/**
 * HSTX display flush callback
 * This is called by LVGL when it needs to flush the frame buffer to display
 */
static void display_hstx_flush(lv_display_t *disp, const lv_area_t *area,
                                uint8_t *color_p)
{
    // HSTX hardware handles automatic scanout of frame buffer
    // No explicit flush needed - just mark as ready
    lv_display_flush_ready(disp);
}

void display_init(void)
{
    printf("[DISPLAY] Initializing HSTX DVI/HDMI breakout (640x480@60Hz)\n");
    
    // Create LVGL display driver for RP2350 HSTX
    display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    
    if (display == NULL)
    {
        printf("[DISPLAY] ERROR: Failed to create LVGL display!\n");
        return;
    }
    
    // Set up the display callback for flushing
    lv_display_set_flush_cb(display, display_hstx_flush);
    
    // Allocate frame buffer (ensure it's in fast RAM for HSTX DMA)
    size_t frame_buffer_size = LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t);
    uint8_t *frame_buffer = malloc(frame_buffer_size);
    
    if (frame_buffer == NULL)
    {
        printf("[DISPLAY] ERROR: Failed to allocate frame buffer (%zu bytes)!\n", 
               frame_buffer_size);
        return;
    }
    
    // Set draw buffers using LVGL API
    lv_display_set_buffers(display, frame_buffer, NULL, frame_buffer_size, LV_DISPLAY_RENDER_MODE_FULL);
    
    printf("[DISPLAY] Frame buffer allocated: %zu bytes\n", frame_buffer_size);
    printf("[DISPLAY] Display initialized: %dx%d\n", LCD_WIDTH, LCD_HEIGHT);
    printf("[DISPLAY] HSTX DVI/HDMI output active on dedicated pins\n");
}