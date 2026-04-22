#include "pio_neopixel.h"
#include "config.h"
#include "neopixel.pio.h"   // auto-generated from neopixel.pio
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

static PIO  np_pio = pio0;
static uint np_sm  = 0;
static uint32_t pixel_buf[NEOPIXEL_COUNT];

void neopixel_init(void) {
    uint offset = pio_add_program(np_pio, &ws2812_program);
    ws2812_program_init(np_pio, np_sm, offset, NEOPIXEL_PIN, 800000, false);
    neopixel_clear();
    neopixel_show();
}

void neopixel_set(uint index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= NEOPIXEL_COUNT) return;
    // NeoPixel expects GRB order, packed into upper 24 bits
    pixel_buf[index] = ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
}

void neopixel_show(void) {
    for (uint i = 0; i < NEOPIXEL_COUNT; i++) {
        pio_sm_put_blocking(np_pio, np_sm, pixel_buf[i]);
    }
    sleep_us(300);  // latch / reset pulse
}

void neopixel_fill(uint8_t r, uint8_t g, uint8_t b) {
    for (uint i = 0; i < NEOPIXEL_COUNT; i++) {
        neopixel_set(i, r, g, b);
    }
}

void neopixel_clear(void) {
    neopixel_fill(0, 0, 0);
}

void neopixel_set_resource(uint8_t percent) {
    // Green → yellow → red based on level
    uint lit = (NEOPIXEL_COUNT * percent) / 100;
    for (uint i = 0; i < NEOPIXEL_COUNT; i++) {
        if (i < lit) {
            if (percent > 60)      neopixel_set(i, 0,   255, 0);    // green
            else if (percent > 30) neopixel_set(i, 255, 165, 0);    // orange
            else                   neopixel_set(i, 255, 0,   0);    // red
        } else {
            neopixel_set(i, 0, 0, 0);
        }
    }
}
