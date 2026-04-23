#define FASTLED_LEAN_AND_MEAN 1
#include "FastLED.h"
#include "config.h"
#include "led_strip.h"
#include "pico/stdlib.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Internal state
// ═══════════════════════════════════════════════════════════════════════════════

// FastLED pixel array (the framebuffer)
static CRGB leds[WS2812_NUM_LEDS];

// Per-LED brightness scaling (0-255), applied on top of global brightness
static uint8_t pixel_brightness[WS2812_NUM_LEDS];

// ─── Color Preset Lookup Table ──────────────────────────────────────────────
static const CRGB color_table[COLOR_COUNT] = {
    CRGB(255,   0,   0),   // COLOR_RED
    CRGB(  0, 255,   0),   // COLOR_GREEN
    CRGB(  0,   0, 255),   // COLOR_BLUE
    CRGB(255, 255,   0),   // COLOR_YELLOW
    CRGB(  0, 255, 255),   // COLOR_CYAN
    CRGB(255,   0, 255),   // COLOR_MAGENTA
    CRGB(255, 165,   0),   // COLOR_ORANGE
    CRGB(128,   0, 128),   // COLOR_PURPLE
    CRGB(255, 255, 255),   // COLOR_WHITE
    CRGB(255, 200, 100),   // COLOR_WARM_WHITE
};

// ═══════════════════════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_init(void) {
    FastLED.addLeds<WS2812B, WS2812_PIN, GRB>(leds, WS2812_NUM_LEDS);
    FastLED.setBrightness(128);  // default 50% brightness

    // All per-LED brightnesses start at full
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        pixel_brightness[i] = 255;
    }

    led_strip_clear();
    led_strip_show();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Global Brightness
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_set_brightness(uint8_t brightness) {
    FastLED.setBrightness(brightness);
}

uint8_t led_strip_get_brightness(void) {
    return FastLED.getBrightness();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Per-LED Color Control
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= WS2812_NUM_LEDS) return;
    leds[index] = CRGB(r, g, b);
}

void led_strip_set_pixel_hsv(uint8_t index, uint8_t h, uint8_t s, uint8_t v) {
    if (index >= WS2812_NUM_LEDS) return;
    leds[index] = CHSV(h, s, v);
}

void led_strip_set_all(uint8_t r, uint8_t g, uint8_t b) {
    CRGB color(r, g, b);
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = color;
    }
}

void led_strip_clear(void) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Per-LED Brightness
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_set_pixel_brightness(uint8_t index, uint8_t brightness) {
    if (index >= WS2812_NUM_LEDS) return;
    pixel_brightness[index] = brightness;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Color Presets
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_set_pixel_color(uint8_t index, LedColor color) {
    if (index >= WS2812_NUM_LEDS || color >= COLOR_COUNT) return;
    leds[index] = color_table[color];
}

void led_strip_set_all_color(LedColor color) {
    if (color >= COLOR_COUNT) return;
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = color_table[color];
    }
}

void led_strip_get_color_rgb(LedColor color, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (color >= COLOR_COUNT) {
        *r = *g = *b = 0;
        return;
    }
    *r = color_table[color].r;
    *g = color_table[color].g;
    *b = color_table[color].b;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Display Update
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_show(void) {
    // Apply per-LED brightness scaling:
    // Save original colors, scale each LED, show, then restore.
    CRGB saved[WS2812_NUM_LEDS];
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        saved[i] = leds[i];
        if (pixel_brightness[i] < 255) {
            leds[i].nscale8(pixel_brightness[i]);
        }
    }

    FastLED.show();

    // Restore original unscaled colors so subsequent set calls aren't
    // cumulative lossy.
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = saved[i];
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Color Sequences / Animations
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_fill_rainbow(uint8_t start_hue, uint8_t hue_step) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = CHSV(start_hue + (i * hue_step), 255, 255);
    }
}

void led_strip_fill_gradient(uint8_t r1, uint8_t g1, uint8_t b1,
                             uint8_t r2, uint8_t g2, uint8_t b2) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        uint8_t blend = (i * 255) / (WS2812_NUM_LEDS - 1);
        uint8_t r = r1 + ((int)(r2 - r1) * blend) / 255;
        uint8_t g = g1 + ((int)(g2 - g1) * blend) / 255;
        uint8_t b = b1 + ((int)(b2 - b1) * blend) / 255;
        leds[i] = CRGB(r, g, b);
    }
}

void led_strip_color_wipe(LedColor color, uint32_t delay_ms) {
    if (color >= COLOR_COUNT) return;
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = color_table[color];
        led_strip_show();
        sleep_ms(delay_ms);
    }
}

void led_strip_theater_chase(LedColor color, uint32_t delay_ms) {
    if (color >= COLOR_COUNT) return;
    CRGB c = color_table[color];
    for (int cycle = 0; cycle < 10; cycle++) {
        for (int offset = 0; offset < 3; offset++) {
            for (int i = 0; i < WS2812_NUM_LEDS; i++) {
                leds[i] = ((i + offset) % 3 == 0) ? c : CRGB::Black;
            }
            led_strip_show();
            sleep_ms(delay_ms);
        }
    }
}

void led_strip_rainbow_cycle(uint32_t delay_ms) {
    for (int j = 0; j < 256; j++) {
        for (int i = 0; i < WS2812_NUM_LEDS; i++) {
            uint8_t hue = ((i * 256 / WS2812_NUM_LEDS) + j) & 0xFF;
            leds[i] = CHSV(hue, 255, 255);
        }
        led_strip_show();
        sleep_ms(delay_ms);
    }
}

void led_strip_color_sequence(const LedColor *colors, uint8_t num_colors,
                              uint32_t delay_ms) {
    for (int c = 0; c < num_colors; c++) {
        if (colors[c] < COLOR_COUNT) {
            led_strip_set_all_color(colors[c]);
            led_strip_show();
            sleep_ms(delay_ms);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Utility
// ═══════════════════════════════════════════════════════════════════════════════

void led_strip_shift_left(void) {
    CRGB first = leds[0];
    for (int i = 0; i < WS2812_NUM_LEDS - 1; i++) {
        leds[i] = leds[i + 1];
    }
    leds[WS2812_NUM_LEDS - 1] = first;
}

void led_strip_shift_right(void) {
    CRGB last = leds[WS2812_NUM_LEDS - 1];
    for (int i = WS2812_NUM_LEDS - 1; i > 0; i--) {
        leds[i] = leds[i - 1];
    }
    leds[0] = last;
}

void led_strip_fade_to_black(uint8_t fade_amount) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i].fadeToBlackBy(fade_amount);
    }
}

