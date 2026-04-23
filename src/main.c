#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "config.h"
#include "spi_display.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/st7796/lv_st7796.h"

static void send_cmd_cb(lv_display_t *disp,
                        const uint8_t *cmd,  uint32_t cmd_len,
                        const uint8_t *param, uint32_t param_len)
{
    gpio_put(SPI_DC_PIN, 0);  // command mode
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_len);

    if (param && param_len) {
        gpio_put(SPI_DC_PIN, 1);  // data mode
        spi_write_blocking(SPI_PORT, param, param_len);
    }
    gpio_put(SPI_CS_PIN, 1);
}

static void send_color_cb(lv_display_t *disp,
                          const uint8_t *cmd,   uint32_t cmd_len,
                          uint8_t       *color, uint32_t color_len)
{
    gpio_put(SPI_DC_PIN, 0);
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_len);

    gpio_put(SPI_DC_PIN, 1);
    spi_write_blocking(SPI_PORT, color, color_len);
    gpio_put(SPI_CS_PIN, 1);

    lv_display_flush_ready(disp);  // tell LVGL we're done
}

int main(void) {
    stdio_init_all();

    // SPI init
    spi_init(SPI_PORT, SPI_BAUD_HZ);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CLK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    
    // Control pins as GPIO outputs
    gpio_init(SPI_CS_PIN);  gpio_set_dir(SPI_CS_PIN,  GPIO_OUT); gpio_put(SPI_CS_PIN,  1);
    gpio_init(SPI_DC_PIN);  gpio_set_dir(SPI_DC_PIN,  GPIO_OUT); gpio_put(SPI_DC_PIN,  1);
    gpio_init(SPI_RST_PIN); gpio_set_dir(SPI_RST_PIN, GPIO_OUT); gpio_put(SPI_RST_PIN, 1);
    gpio_init(SPI_BL_PIN);  gpio_set_dir(SPI_BL_PIN,  GPIO_OUT); gpio_put(SPI_BL_PIN,  1);

    // Hardware reset sequence
    gpio_put(SPI_RST_PIN, 1); sleep_ms(50);
    gpio_put(SPI_RST_PIN, 0); sleep_ms(200);
    gpio_put(SPI_RST_PIN, 1); sleep_ms(200);

    lv_init();

    // Create display with LVGL ST7796 driver
    lv_display_t *disp = lv_st7796_create(LCD_WIDTH, LCD_HEIGHT, LV_LCD_FLAG_NONE,
                                           send_cmd_cb, send_color_cb);

    // Allocate draw buffers (1/10 of screen size)
    uint32_t buf_size = LCD_WIDTH * LCD_HEIGHT * 2 / 10;
    void * buf1 = malloc(buf_size);
    void * buf2 = malloc(buf_size);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Set screen background to black
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    // Create label with white text
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello RP2350!");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);

    while (1) {
        lv_timer_handler();
        sleep_ms(5);
    }
}