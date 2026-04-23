#include "spi_display.h"
#include "config.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdlib.h>

// LVGL specific dependencies
#include "lvgl.h"
#include "lvgl/src/drivers/display/st7796/lv_st7796.h"

// ── LVGL Callbacks ────────────────────────────────────────────────────────────

static void display_send_cmd(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size, const uint8_t *param, size_t param_size)
{
    gpio_put(SPI_DC_PIN, 0); // DC low = command
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_size);

    if (param_size > 0)
    {
        gpio_put(SPI_DC_PIN, 1); // DC high = data
        spi_write_blocking(SPI_PORT, param, param_size);
    }

    gpio_put(SPI_CS_PIN, 1);
}

static void display_send_color(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size, uint8_t *param, size_t param_size)
{
    gpio_put(SPI_DC_PIN, 0); // DC low = command
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_size);

    if (param_size > 0)
    {
        gpio_put(SPI_DC_PIN, 1); // DC high = data
        spi_write_blocking(SPI_PORT, param, param_size);
    }

    gpio_put(SPI_CS_PIN, 1);

    /* For blocking transfer, we must tell LVGL that we are ready */
    lv_display_flush_ready(disp);
}

// ── Public API ────────────────────────────────────────────────────────────────

static uint32_t systick_get_cb(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

static void sysdelay_cb(uint32_t ms) {
    sleep_ms(ms);
}

void display_init(void)
{
    // Initialize LVGL core
    lv_init();
    lv_tick_set_cb(systick_get_cb);
    lv_delay_set_cb(sysdelay_cb);
    // SPI peripheral init
    spi_init(SPI_PORT, SPI_BAUD_HZ);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SPI_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);

    // Control pins as GPIO outputs
    gpio_init(SPI_CS_PIN);
    gpio_set_dir(SPI_CS_PIN, GPIO_OUT);
    gpio_put(SPI_CS_PIN, 1);
    gpio_init(SPI_DC_PIN);
    gpio_set_dir(SPI_DC_PIN, GPIO_OUT);
    gpio_put(SPI_DC_PIN, 1);
    gpio_init(SPI_RST_PIN);
    gpio_set_dir(SPI_RST_PIN, GPIO_OUT);
    gpio_put(SPI_RST_PIN, 1);
    gpio_init(SPI_BL_PIN);
    gpio_set_dir(SPI_BL_PIN, GPIO_OUT);
    gpio_put(SPI_BL_PIN, 1);

    // Hardware reset
    gpio_put(SPI_RST_PIN, 1);
    sleep_ms(50);
    gpio_put(SPI_RST_PIN, 0);
    sleep_ms(200);
    gpio_put(SPI_RST_PIN, 1);
    sleep_ms(200);

    // LVGL ST7796 Display creation
    lv_display_t *disp = lv_st7796_create(LCD_HEIGHT, LCD_WIDTH, 0, display_send_cmd, display_send_color);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

    // Allocate draw buffers (e.g. 1/10 of the screen size)
    uint32_t buf_size = LCD_WIDTH * LCD_HEIGHT * 2 / 10;
    void *buf1 = malloc(buf_size);
    void *buf2 = malloc(buf_size);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
}