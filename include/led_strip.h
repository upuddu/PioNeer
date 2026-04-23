#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Color Presets (10 colors) ──────────────────────────────────────
typedef enum LedColor {
    COLOR_RED = 0,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_ORANGE,
    COLOR_PURPLE,
    COLOR_WHITE,
    COLOR_WARM_WHITE,
    COLOR_COUNT
} LedColor;

// ─── Initialization ────────────────────────────────────────────────
// Sets up the FastLED WS2812B controller on GPIO9, clears all LEDs.
void led_strip_init(void);

// ─── Global Brightness ─────────────────────────────────────────────
// Set/get the global brightness applied to all LEDs (0-255).
void led_strip_set_brightness(uint8_t brightness);
uint8_t led_strip_get_brightness(void);

// ─── Per-LED Color Control ──────────────────────────────────────────
// Set a single LED's color by RGB values.
void led_strip_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

// Set a single LED's color by HSV values (hue 0-255, sat 0-255, val 0-255).
void led_strip_set_pixel_hsv(uint8_t index, uint8_t h, uint8_t s, uint8_t v);

// Set all LEDs to the same RGB color.
void led_strip_set_all(uint8_t r, uint8_t g, uint8_t b);

// Turn all LEDs off (set to black).
void led_strip_clear(void);

// ─── Per-LED Brightness ────────────────────────────────────────────
// Set an individual LED's brightness scaling (0-255).
// This is multiplied with the global brightness on show().
void led_strip_set_pixel_brightness(uint8_t index, uint8_t brightness);

// ─── Per-LED Color Control (Extended) ──────────────────────────────
// Get the current RGB values of a pixel.
void led_strip_get_pixel_rgb(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b);

// Set a range of LEDs to the same RGB color.
void led_strip_set_range_rgb(uint8_t start, uint8_t count, uint8_t r, uint8_t g, uint8_t b);

// Set a range of LEDs to the same preset color.
void led_strip_set_range_color(uint8_t start, uint8_t count, LedColor color);

// Clear a range of LEDs (set to black).
void led_strip_clear_range(uint8_t start, uint8_t count);

// Copy color from one pixel to another.
void led_strip_copy_pixel(uint8_t from_index, uint8_t to_index);

// Swap colors between two pixels.
void led_strip_swap_pixels(uint8_t index1, uint8_t index2);

// ─── Pattern and Effect Functions ──────────────────────────────────
// Fill the strip with a gradient between two RGB colors.
void led_strip_gradient_fill(uint8_t r1, uint8_t g1, uint8_t b1,
                             uint8_t r2, uint8_t g2, uint8_t b2);

// Create a wave effect with a color moving across the strip.
void led_strip_wave(LedColor color, uint8_t width, uint32_t delay_ms);

// Bounce effect: light moves back and forth across the strip.
void led_strip_bounce(LedColor color, uint32_t delay_ms);

// Chase effect: lights chase each other across the strip.
void led_strip_chase(LedColor color, uint8_t spacing, uint32_t delay_ms);

// Pulse all LEDs to a color and back to black.
void led_strip_pulse_all(LedColor color, uint32_t duration_ms);

// Pulse a single pixel to a color and back.
void led_strip_pulse_pixel(uint8_t index, LedColor color, uint32_t duration_ms);

// Blink a pixel on/off with a color.
void led_strip_blink_pixel(uint8_t index, LedColor color, uint32_t on_time_ms, uint32_t off_time_ms, uint8_t cycles);

// Fill strip with random colors.
void led_strip_random_fill(void);

// Set a random pixel to a random color.
void led_strip_random_pixel(void);

// ─── Utility Functions ─────────────────────────────────────────────
// Reverse the order of LEDs in the strip.
void led_strip_reverse(void);

// Mirror the strip (copy left half to right half).
void led_strip_mirror(void);

// Rotate all pixels left by one position.
void led_strip_rotate_left(void);

// Rotate all pixels right by one position.
void led_strip_rotate_right(void);

// Fade all pixels toward black by fade_amount.
void led_strip_fade_all(uint8_t fade_amount);

// ─── Color Presets ──────────────────────────────────────────────────
// Set a single LED to a preset color.
void led_strip_set_pixel_color(uint8_t index, LedColor color);

// Set all LEDs to a preset color.
void led_strip_set_all_color(LedColor color);

// Get the RGB values for a preset color.
void led_strip_get_color_rgb(LedColor color, uint8_t *r, uint8_t *g, uint8_t *b);

// ─── Display Update ─────────────────────────────────────────────────
// Push the current framebuffer to the LED strip via PIO.
// Applies per-LED and global brightness scaling before sending.
void led_strip_show(void);

// ─── Color Sequences / Animations (blocking) ───────────────────────
// Fill with a rainbow gradient starting from start_hue, stepping by hue_step.
void led_strip_fill_rainbow(uint8_t start_hue, uint8_t hue_step);

// Fill with a linear gradient between two RGB colors.
void led_strip_fill_gradient(uint8_t r1, uint8_t g1, uint8_t b1,
                             uint8_t r2, uint8_t g2, uint8_t b2);

// Wipe a preset color across the strip one LED at a time.
void led_strip_color_wipe(LedColor color, uint32_t delay_ms);

// Theater-chase animation with a preset color.
void led_strip_theater_chase(LedColor color, uint32_t delay_ms);

// Rainbow cycle animation (one full cycle).
void led_strip_rainbow_cycle(uint32_t delay_ms);

// Rainbow loading animation (fills strip progressively with rainbow colors).
void led_strip_rainbow_loading(uint32_t delay_ms);

// Cycle through an array of preset colors, lighting up the full strip each time.
void led_strip_color_sequence(const LedColor *colors, uint8_t num_colors,
                              uint32_t delay_ms);

// ─── Utility ────────────────────────────────────────────────────────
// Shift all pixel colors left by one position (pixel 0 wraps to end).
void led_strip_shift_left(void);

// Shift all pixel colors right by one position (last pixel wraps to 0).
void led_strip_shift_right(void);

// Dim all LEDs toward black by fade_amount (subtracted from each channel).
void led_strip_fade_to_black(uint8_t fade_amount);

#ifdef __cplusplus
}
#endif

#endif // LED_STRIP_H
