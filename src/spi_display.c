#include "spi_display.h"
#include "config.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

// ── Low-level helpers ─────────────────────────────────────────────────────────

static void display_write_cmd(uint8_t cmd) {
    gpio_put(SPI_DC_PIN, 0);    // DC low = command
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(SPI_CS_PIN, 1);
}

static void display_write_data(uint8_t data) {
    gpio_put(SPI_DC_PIN, 1);    // DC high = data
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(SPI_CS_PIN, 1);
}

// ── Public API ────────────────────────────────────────────────────────────────

void display_init(void) {
    // SPI init
    spi_init(SPI_PORT, SPI_BAUD_HZ);
    gpio_set_function(SPI_CLK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);

    // CS, DC, RST as GPIO outputs
    gpio_init(SPI_CS_PIN);  gpio_set_dir(SPI_CS_PIN,  GPIO_OUT); gpio_put(SPI_CS_PIN,  1);
    gpio_init(SPI_DC_PIN);  gpio_set_dir(SPI_DC_PIN,  GPIO_OUT); gpio_put(SPI_DC_PIN,  1);
    gpio_init(SPI_RST_PIN); gpio_set_dir(SPI_RST_PIN, GPIO_OUT); gpio_put(SPI_RST_PIN, 1);

    // Hardware reset
    gpio_put(SPI_RST_PIN, 0); sleep_ms(10);
    gpio_put(SPI_RST_PIN, 1); sleep_ms(120);

    // TODO: Add your TFT controller init sequence (ST7789 / ILI9341 / etc.)
    // Example for ILI9341:
    // display_write_cmd(0x01);  // software reset
    // sleep_ms(100);
    // display_write_cmd(0x11);  // sleep out
    // ...
}

void display_clear(uint16_t color) {
    // TODO: set address window to full screen, flood fill with color
    (void)color;
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    // TODO: set address window to (x,y), write color word
    (void)x; (void)y; (void)color;
}

void display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    for (uint16_t row = y; row < y + h; row++) {
        for (uint16_t col = x; col < x + w; col++) {
            display_draw_pixel(col, row, color);
        }
    }
}

void display_write_string(uint16_t x, uint16_t y, const char *str, uint16_t color) {
    // TODO: implement font rendering or use a font library
    (void)x; (void)y; (void)str; (void)color;
}
