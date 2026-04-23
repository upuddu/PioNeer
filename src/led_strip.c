#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "ws2812.pio.h"
#include "config.h"
#include "led_strip.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

static rgb_t leds[WS2812_NUM_LEDS];
static uint8_t pixel_brightness[WS2812_NUM_LEDS];
static uint8_t global_brightness = 128;

static PIO ws2812_pio = pio0;
static uint ws2812_sm = 0;
static uint ws2812_offset = 0;

static inline uint8_t scale8(uint8_t value, uint8_t scale) {
    return (uint8_t)(((uint16_t)value * (uint16_t)scale + 0x80) >> 8);
}

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    // MSB-first output: put colors in upper 24 bits for correct bit ordering
    return ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
}

static void hsv_to_rgb(uint8_t hue, uint8_t sat, uint8_t val,
                       uint8_t *out_r, uint8_t *out_g, uint8_t *out_b) {
    if (sat == 0) {
        *out_r = val;
        *out_g = val;
        *out_b = val;
        return;
    }

    uint16_t region = hue / 43;
    uint16_t remainder = (hue - (region * 43)) * 6;
    uint16_t p = (val * (255 - sat)) >> 8;
    uint16_t q = (val * (255 - ((sat * remainder) >> 8))) >> 8;
    uint16_t t = (val * (255 - ((sat * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            *out_r = val;
            *out_g = t;
            *out_b = p;
            break;
        case 1:
            *out_r = q;
            *out_g = val;
            *out_b = p;
            break;
        case 2:
            *out_r = p;
            *out_g = val;
            *out_b = t;
            break;
        case 3:
            *out_r = p;
            *out_g = q;
            *out_b = val;
            break;
        case 4:
            *out_r = t;
            *out_g = p;
            *out_b = val;
            break;
        default:
            *out_r = val;
            *out_g = p;
            *out_b = q;
            break;
    }
}

void led_strip_init(void) {
    ws2812_offset = pio_add_program(ws2812_pio, &ws2812_program);
    
    // Manually initialize with corrected shift direction (MSB-first for WS2812B)
    pio_gpio_init(ws2812_pio, WS2812_PIN);
    pio_sm_set_consecutive_pindirs(ws2812_pio, ws2812_sm, WS2812_PIN, 1, true);
    
    pio_sm_config c = ws2812_program_get_default_config(ws2812_offset);
    sm_config_set_sideset_pins(&c, WS2812_PIN);
    // CRITICAL: shift_right=TRUE (MSB-first) for correct WS2812B bit order
    sm_config_set_out_shift(&c, true, true, 24);  
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    
    int cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3;
    float div = (float)clock_get_hz(clk_sys) / (800000.0f * (float)cycles_per_bit);
    sm_config_set_clkdiv(&c, div);
    
    pio_sm_init(ws2812_pio, ws2812_sm, ws2812_offset, &c);
    pio_sm_set_enabled(ws2812_pio, ws2812_sm, true);

    global_brightness = 128;
    memset(pixel_brightness, 0xFF, sizeof(pixel_brightness));
    led_strip_clear();
    led_strip_show();
    
    printf("LED strip initialized on GPIO%d (MSB-first PIO shift)\n", WS2812_PIN);
}

void led_strip_set_brightness(uint8_t brightness) {
    global_brightness = brightness;
}

uint8_t led_strip_get_brightness(void) {
    return global_brightness;
}

void led_strip_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= WS2812_NUM_LEDS) return;
    leds[index].r = r;
    leds[index].g = g;
    leds[index].b = b;
}

void led_strip_set_pixel_hsv(uint8_t index, uint8_t h, uint8_t s, uint8_t v) {
    if (index >= WS2812_NUM_LEDS) return;
    hsv_to_rgb(h, s, v, &leds[index].r, &leds[index].g, &leds[index].b);
}

void led_strip_set_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i].r = r;
        leds[i].g = g;
        leds[i].b = b;
    }
}

void led_strip_clear(void) {
    memset(leds, 0, sizeof(leds));
}

void led_strip_set_pixel_brightness(uint8_t index, uint8_t brightness) {
    if (index >= WS2812_NUM_LEDS) return;
    pixel_brightness[index] = brightness;
}

uint8_t led_strip_get_pixel_brightness(uint8_t index) {
    if (index >= WS2812_NUM_LEDS) return 0;
    return pixel_brightness[index];
}

void led_strip_set_pixel_color_brightness(uint8_t index, LedColor color, uint8_t brightness) {
    led_strip_set_pixel_color(index, color);
    led_strip_set_pixel_brightness(index, brightness);
}

void led_strip_set_pixel_rgb_brightness(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    led_strip_set_pixel(index, r, g, b);
    led_strip_set_pixel_brightness(index, brightness);
}

void led_strip_dim_pixel(uint8_t index, uint8_t amount) {
    if (index >= WS2812_NUM_LEDS) return;
    pixel_brightness[index] = (pixel_brightness[index] > amount)
                              ? pixel_brightness[index] - amount : 0;
}

void led_strip_set_brightness_range(uint8_t start, uint8_t count, uint8_t brightness) {
    if (start >= WS2812_NUM_LEDS) return;
    uint8_t end = (start + count > WS2812_NUM_LEDS) ? WS2812_NUM_LEDS : start + count;
    for (uint8_t i = start; i < end; i++) {
        pixel_brightness[i] = brightness;
    }
}

void led_strip_dim_range(uint8_t start, uint8_t count, uint8_t amount) {
    if (start >= WS2812_NUM_LEDS) return;
    uint8_t end = (start + count > WS2812_NUM_LEDS) ? WS2812_NUM_LEDS : start + count;
    for (uint8_t i = start; i < end; i++) {
        pixel_brightness[i] = (pixel_brightness[i] > amount)
                              ? pixel_brightness[i] - amount : 0;
    }
}

void led_strip_reset_all_brightness(void) {
    memset(pixel_brightness, 0xFF, sizeof(pixel_brightness));
}

static const rgb_t color_table[COLOR_COUNT] = {
    {255,   0,   0},
    {  0, 255,   0},
    {  0,   0, 255},
    {255, 255,   0},
    {  0, 255, 255},
    {255,   0, 255},
    {255, 165,   0},
    {128,   0, 128},
    {255, 255, 255},
    {255, 200, 100},
};

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

void led_strip_show(void) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        uint8_t r = scale8(leds[i].r, global_brightness);
        uint8_t g = scale8(leds[i].g, global_brightness);
        uint8_t b = scale8(leds[i].b, global_brightness);

        r = scale8(r, pixel_brightness[i]);
        g = scale8(g, pixel_brightness[i]);
        b = scale8(b, pixel_brightness[i]);

        // MSB-first format for PIO output
        uint32_t grb = ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
        pio_sm_put_blocking(ws2812_pio, ws2812_sm, grb);
    }
    sleep_us(80);
}

void led_strip_fill_rainbow(uint8_t start_hue, uint8_t hue_step) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        uint8_t hue = (uint8_t)(start_hue + (uint8_t)(i * hue_step));
        hsv_to_rgb(hue, 255, 255, &leds[i].r, &leds[i].g, &leds[i].b);
    }
}

void led_strip_fill_gradient(uint8_t r1, uint8_t g1, uint8_t b1,
                             uint8_t r2, uint8_t g2, uint8_t b2) {
    if (WS2812_NUM_LEDS == 0) return;
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        uint8_t blend = (uint8_t)((i * 255) / (WS2812_NUM_LEDS - 1));
        leds[i].r = (uint8_t)(r1 + ((int)(r2 - r1) * blend) / 255);
        leds[i].g = (uint8_t)(g1 + ((int)(g2 - g1) * blend) / 255);
        leds[i].b = (uint8_t)(b1 + ((int)(b2 - b1) * blend) / 255);
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
    rgb_t c = color_table[color];
    for (int cycle = 0; cycle < 10; cycle++) {
        for (int offset = 0; offset < 3; offset++) {
            for (int i = 0; i < WS2812_NUM_LEDS; i++) {
                leds[i] = ((i + offset) % 3 == 0) ? c : (rgb_t){0, 0, 0};
            }
            led_strip_show();
            sleep_ms(delay_ms);
        }
    }
}

void led_strip_rainbow_cycle(uint32_t delay_ms) {
    for (int j = 0; j < 256; j++) {
        for (int i = 0; i < WS2812_NUM_LEDS; i++) {
            uint8_t hue = (uint8_t)(((i * 256) / WS2812_NUM_LEDS) + j);
            hsv_to_rgb(hue, 255, 255, &leds[i].r, &leds[i].g, &leds[i].b);
        }
        led_strip_show();
        sleep_ms(delay_ms);
    }
}

void led_strip_rainbow_loading(uint32_t delay_ms) {
    // Fill the strip progressively with rainbow colors (loading bar effect)
    for (int fill = 0; fill <= WS2812_NUM_LEDS; fill++) {
        for (int i = 0; i < WS2812_NUM_LEDS; i++) {
            if (i < fill) {
                // Rainbow color based on position
                uint8_t hue = (uint8_t)((i * 256) / WS2812_NUM_LEDS);
                hsv_to_rgb(hue, 255, 255, &leds[i].r, &leds[i].g, &leds[i].b);
            } else {
                // Turn off LEDs beyond the fill point
                leds[i] = (rgb_t){0, 0, 0};
            }
        }
        led_strip_show();
        sleep_ms(delay_ms);
    }
    // Optional: Keep the full rainbow for a moment
    sleep_ms(delay_ms * 2);
}

void led_strip_color_sequence(const LedColor *colors, uint8_t num_colors,
                              uint32_t delay_ms) {
    for (uint8_t c = 0; c < num_colors; c++) {
        if (colors[c] < COLOR_COUNT) {
            led_strip_set_all_color(colors[c]);
            led_strip_show();
            sleep_ms(delay_ms);
        }
    }
}

void led_strip_shift_left(void) {
    if (WS2812_NUM_LEDS == 0) return;
    rgb_t first = leds[0];
    for (int i = 0; i < WS2812_NUM_LEDS - 1; i++) {
        leds[i] = leds[i + 1];
    }
    leds[WS2812_NUM_LEDS - 1] = first;
}

void led_strip_shift_right(void) {
    if (WS2812_NUM_LEDS == 0) return;
    rgb_t last = leds[WS2812_NUM_LEDS - 1];
    for (int i = WS2812_NUM_LEDS - 1; i > 0; i--) {
        leds[i] = leds[i - 1];
    }
    leds[0] = last;
}

void led_strip_fade_to_black(uint8_t fade_amount) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i].r = (uint8_t)(leds[i].r > fade_amount ? leds[i].r - fade_amount : 0);
        leds[i].g = (uint8_t)(leds[i].g > fade_amount ? leds[i].g - fade_amount : 0);
        leds[i].b = (uint8_t)(leds[i].b > fade_amount ? leds[i].b - fade_amount : 0);
    }
}

// ─── Extended Per-LED Control Functions ─────────────────────────────

void led_strip_get_pixel_rgb(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (index >= WS2812_NUM_LEDS || !r || !g || !b) return;
    *r = leds[index].r;
    *g = leds[index].g;
    *b = leds[index].b;
}

void led_strip_set_range_rgb(uint8_t start, uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
    if (start >= WS2812_NUM_LEDS) return;
    uint8_t end = (start + count > WS2812_NUM_LEDS) ? WS2812_NUM_LEDS : start + count;
    for (uint8_t i = start; i < end; i++) {
        leds[i].r = r;
        leds[i].g = g;
        leds[i].b = b;
    }
}

void led_strip_set_range_color(uint8_t start, uint8_t count, LedColor color) {
    if (color >= COLOR_COUNT) return;
    led_strip_set_range_rgb(start, count, color_table[color].r, color_table[color].g, color_table[color].b);
}

void led_strip_clear_range(uint8_t start, uint8_t count) {
    led_strip_set_range_rgb(start, count, 0, 0, 0);
}

void led_strip_copy_pixel(uint8_t from_index, uint8_t to_index) {
    if (from_index >= WS2812_NUM_LEDS || to_index >= WS2812_NUM_LEDS) return;
    leds[to_index] = leds[from_index];
}

void led_strip_swap_pixels(uint8_t index1, uint8_t index2) {
    if (index1 >= WS2812_NUM_LEDS || index2 >= WS2812_NUM_LEDS) return;
    rgb_t temp = leds[index1];
    leds[index1] = leds[index2];
    leds[index2] = temp;
}

// ─── Pattern and Effect Functions ───────────────────────────────────

void led_strip_gradient_fill(uint8_t r1, uint8_t g1, uint8_t b1,
                             uint8_t r2, uint8_t g2, uint8_t b2) {
    if (WS2812_NUM_LEDS == 0) return;
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        uint8_t blend = (uint8_t)((i * 255) / (WS2812_NUM_LEDS - 1));
        leds[i].r = (uint8_t)(r1 + ((int)(r2 - r1) * blend) / 255);
        leds[i].g = (uint8_t)(g1 + ((int)(g2 - g1) * blend) / 255);
        leds[i].b = (uint8_t)(b1 + ((int)(b2 - b1) * blend) / 255);
    }
}

void led_strip_wave(LedColor color, uint8_t width, uint32_t delay_ms) {
    if (color >= COLOR_COUNT || width == 0) return;
    rgb_t c = color_table[color];
    
    for (int pos = -width; pos < WS2812_NUM_LEDS + width; pos++) {
        led_strip_clear();
        for (int i = 0; i < width; i++) {
            int led_pos = pos + i;
            if (led_pos >= 0 && led_pos < WS2812_NUM_LEDS) {
                leds[led_pos] = c;
            }
        }
        led_strip_show();
        sleep_ms(delay_ms);
    }
}

void led_strip_bounce(LedColor color, uint32_t delay_ms) {
    if (color >= COLOR_COUNT) return;
    rgb_t c = color_table[color];
    
    // Forward
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        led_strip_clear();
        leds[i] = c;
        led_strip_show();
        sleep_ms(delay_ms);
    }
    // Backward
    for (int i = WS2812_NUM_LEDS - 2; i > 0; i--) {
        led_strip_clear();
        leds[i] = c;
        led_strip_show();
        sleep_ms(delay_ms);
    }
}

void led_strip_chase(LedColor color, uint8_t spacing, uint32_t delay_ms) {
    if (color >= COLOR_COUNT || spacing == 0) return;
    rgb_t c = color_table[color];
    
    for (int offset = 0; offset < spacing; offset++) {
        for (int cycle = 0; cycle < 5; cycle++) {  // 5 cycles
            for (int i = 0; i < WS2812_NUM_LEDS; i++) {
                leds[i] = ((i + offset) % spacing == 0) ? c : (rgb_t){0, 0, 0};
            }
            led_strip_show();
            sleep_ms(delay_ms);
        }
    }
}

void led_strip_pulse_all(LedColor color, uint32_t duration_ms) {
    if (color >= COLOR_COUNT) return;
    rgb_t c = color_table[color];
    
    // Fade up
    for (int b = 0; b <= 255; b += 5) {
        for (int i = 0; i < WS2812_NUM_LEDS; i++) {
            leds[i].r = (uint8_t)((c.r * b) / 255);
            leds[i].g = (uint8_t)((c.g * b) / 255);
            leds[i].b = (uint8_t)((c.b * b) / 255);
        }
        led_strip_show();
        sleep_ms(duration_ms / (255 / 5) / 2);
    }
    // Fade down
    for (int b = 255; b >= 0; b -= 5) {
        for (int i = 0; i < WS2812_NUM_LEDS; i++) {
            leds[i].r = (uint8_t)((c.r * b) / 255);
            leds[i].g = (uint8_t)((c.g * b) / 255);
            leds[i].b = (uint8_t)((c.b * b) / 255);
        }
        led_strip_show();
        sleep_ms(duration_ms / (255 / 5) / 2);
    }
}

void led_strip_pulse_pixel(uint8_t index, LedColor color, uint32_t duration_ms) {
    if (index >= WS2812_NUM_LEDS || color >= COLOR_COUNT) return;
    rgb_t original = leds[index];
    rgb_t c = color_table[color];
    
    // Fade up
    for (int b = 0; b <= 255; b += 5) {
        leds[index].r = (uint8_t)((c.r * b) / 255);
        leds[index].g = (uint8_t)((c.g * b) / 255);
        leds[index].b = (uint8_t)((c.b * b) / 255);
        led_strip_show();
        sleep_ms(duration_ms / (255 / 5) / 2);
    }
    // Fade down
    for (int b = 255; b >= 0; b -= 5) {
        leds[index].r = (uint8_t)((c.r * b) / 255);
        leds[index].g = (uint8_t)((c.g * b) / 255);
        leds[index].b = (uint8_t)((c.b * b) / 255);
        led_strip_show();
        sleep_ms(duration_ms / (255 / 5) / 2);
    }
    leds[index] = original;
}

void led_strip_blink_pixel(uint8_t index, LedColor color, uint32_t on_time_ms, uint32_t off_time_ms, uint8_t cycles) {
    if (index >= WS2812_NUM_LEDS || color >= COLOR_COUNT) return;
    rgb_t original = leds[index];
    rgb_t c = color_table[color];
    
    for (uint8_t i = 0; i < cycles; i++) {
        leds[index] = c;
        led_strip_show();
        sleep_ms(on_time_ms);
        leds[index] = (rgb_t){0, 0, 0};
        led_strip_show();
        sleep_ms(off_time_ms);
    }
    leds[index] = original;
}

void led_strip_random_fill(void) {
    for (int i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i].r = (uint8_t)(rand() % 256);
        leds[i].g = (uint8_t)(rand() % 256);
        leds[i].b = (uint8_t)(rand() % 256);
    }
}

void led_strip_random_pixel(void) {
    uint8_t index = (uint8_t)(rand() % WS2812_NUM_LEDS);
    leds[index].r = (uint8_t)(rand() % 256);
    leds[index].g = (uint8_t)(rand() % 256);
    leds[index].b = (uint8_t)(rand() % 256);
}

// ─── Utility Functions ──────────────────────────────────────────────

void led_strip_reverse(void) {
    for (int i = 0; i < WS2812_NUM_LEDS / 2; i++) {
        rgb_t temp = leds[i];
        leds[i] = leds[WS2812_NUM_LEDS - 1 - i];
        leds[WS2812_NUM_LEDS - 1 - i] = temp;
    }
}

void led_strip_mirror(void) {
    for (int i = 0; i < WS2812_NUM_LEDS / 2; i++) {
        leds[WS2812_NUM_LEDS - 1 - i] = leds[i];
    }
}

void led_strip_rotate_left(void) {
    if (WS2812_NUM_LEDS == 0) return;
    rgb_t first = leds[0];
    for (int i = 0; i < WS2812_NUM_LEDS - 1; i++) {
        leds[i] = leds[i + 1];
    }
    leds[WS2812_NUM_LEDS - 1] = first;
}

void led_strip_rotate_right(void) {
    if (WS2812_NUM_LEDS == 0) return;
    rgb_t last = leds[WS2812_NUM_LEDS - 1];
    for (int i = WS2812_NUM_LEDS - 1; i > 0; i--) {
        leds[i] = leds[i - 1];
    }
    leds[0] = last;
}

void led_strip_fade_all(uint8_t fade_amount) {
    led_strip_fade_to_black(fade_amount);
}

// ─── Pattern Creation Helpers ────────────────────────────────────────

void led_strip_alternating(LedColor color1, LedColor color2) {
    if (color1 >= COLOR_COUNT || color2 >= COLOR_COUNT) return;
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = (i % 2 == 0) ? color_table[color1] : color_table[color2];
    }
}

void led_strip_set_every_n(LedColor color, uint8_t n, uint8_t offset) {
    if (color >= COLOR_COUNT || n == 0) return;
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
        leds[i] = ((i + offset) % n == 0) ? color_table[color] : (rgb_t){0, 0, 0};
    }
}

void led_strip_set_segment(uint8_t start, uint8_t end, uint8_t r, uint8_t g, uint8_t b) {
    if (start >= WS2812_NUM_LEDS) return;
    if (end >= WS2812_NUM_LEDS) end = WS2812_NUM_LEDS - 1;
    for (uint8_t i = start; i <= end; i++) {
        leds[i].r = r;
        leds[i].g = g;
        leds[i].b = b;
    }
}

void led_strip_apply_color_map(const LedColor *map, uint8_t len) {
    if (!map) return;
    uint8_t count = (len < WS2812_NUM_LEDS) ? len : WS2812_NUM_LEDS;
    for (uint8_t i = 0; i < count; i++) {
        if (map[i] < COLOR_COUNT) {
            leds[i] = color_table[map[i]];
        }
    }
}

// ─── New Animation / Effect Functions ───────────────────────────────

void led_strip_sparkle(LedColor color, uint8_t count, uint32_t delay_ms) {
    if (color >= COLOR_COUNT || count == 0) return;
    // Save current state
    rgb_t saved[WS2812_NUM_LEDS];
    memcpy(saved, leds, sizeof(leds));

    // Randomly light `count` LEDs
    for (uint8_t k = 0; k < count; k++) {
        uint8_t idx = (uint8_t)(rand() % WS2812_NUM_LEDS);
        leds[idx] = color_table[color];
    }
    led_strip_show();
    sleep_ms(delay_ms);

    // Restore original state
    memcpy(leds, saved, sizeof(leds));
    led_strip_show();
}

void led_strip_strobe(LedColor color, uint8_t flashes, uint32_t delay_ms) {
    if (color >= COLOR_COUNT) return;
    for (uint8_t i = 0; i < flashes; i++) {
        led_strip_set_all_color(color);
        led_strip_show();
        sleep_ms(delay_ms);
        led_strip_clear();
        led_strip_show();
        sleep_ms(delay_ms);
    }
}

void led_strip_meteor(LedColor color, uint8_t size, uint8_t fade_amount, uint32_t delay_ms) {
    if (color >= COLOR_COUNT || size == 0) return;
    rgb_t c = color_table[color];

    for (int pos = 0; pos < WS2812_NUM_LEDS + WS2812_NUM_LEDS; pos++) {
        // Fade all existing pixels toward black
        for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
            leds[i].r = (uint8_t)(leds[i].r > fade_amount ? leds[i].r - fade_amount : 0);
            leds[i].g = (uint8_t)(leds[i].g > fade_amount ? leds[i].g - fade_amount : 0);
            leds[i].b = (uint8_t)(leds[i].b > fade_amount ? leds[i].b - fade_amount : 0);
        }
        // Draw meteor head
        for (uint8_t j = 0; j < size; j++) {
            int led_pos = pos - j;
            if (led_pos >= 0 && led_pos < WS2812_NUM_LEDS) {
                leds[led_pos] = c;
            }
        }
        led_strip_show();
        sleep_ms(delay_ms);
    }
}

void led_strip_twinkle_random(uint8_t count, uint32_t delay_ms) {
    if (count == 0) return;
    // Save state
    rgb_t saved[WS2812_NUM_LEDS];
    memcpy(saved, leds, sizeof(leds));

    // Light random pixels with random colors
    for (uint8_t k = 0; k < count; k++) {
        uint8_t idx = (uint8_t)(rand() % WS2812_NUM_LEDS);
        leds[idx].r = (uint8_t)(rand() % 256);
        leds[idx].g = (uint8_t)(rand() % 256);
        leds[idx].b = (uint8_t)(rand() % 256);
    }
    led_strip_show();
    sleep_ms(delay_ms);

    // Fade the twinkling pixels out gently
    for (int step = 5; step >= 0; step--) {
        for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++) {
            leds[i].r = (uint8_t)((leds[i].r * step) / 5);
            leds[i].g = (uint8_t)((leds[i].g * step) / 5);
            leds[i].b = (uint8_t)((leds[i].b * step) / 5);
        }
        led_strip_show();
        sleep_ms(delay_ms / 6);
    }

    // Restore original state
    memcpy(leds, saved, sizeof(leds));
    led_strip_show();
}
