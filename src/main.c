#include "config.h"
#include "hardware/gpio.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/st7796/lv_st7796.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>

static void spi_bb_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        gpio_put(SPI_MOSI_PIN, (data & 0x80) ? 1 : 0);
        data <<= 1;
        gpio_put(SPI_CLK_PIN, 1);
        gpio_put(SPI_CLK_PIN, 0);
    }
}

static void send_cmd_cb(lv_display_t *disp,
                        const uint8_t *cmd,  uint32_t cmd_len,
                        const uint8_t *param, uint32_t param_len)
{
    gpio_put(SPI_DC_PIN, 0);  // command mode
    gpio_put(SPI_CS_PIN, 0);
    for (uint32_t i = 0; i < cmd_len; i++) {
        spi_bb_write_byte(cmd[i]);
    }

    if (param && param_len) {
        gpio_put(SPI_DC_PIN, 1);  // data mode
        for (uint32_t i = 0; i < param_len; i++) {
            spi_bb_write_byte(param[i]);
        }
    }
    gpio_put(SPI_CS_PIN, 1);
}

static void send_color_cb(lv_display_t *disp,
                          const uint8_t *cmd,   uint32_t cmd_len,
                          uint8_t       *color, uint32_t color_len)
{
    gpio_put(SPI_DC_PIN, 0);
    gpio_put(SPI_CS_PIN, 0);
    for (uint32_t i = 0; i < cmd_len; i++) {
        spi_bb_write_byte(cmd[i]);
    }

    gpio_put(SPI_DC_PIN, 1);
    // Swap bytes for 16-bit color (ST7796 expects MSB first, Pico is little-endian)
    for (uint32_t i = 0; i < color_len; i += 2) {
        spi_bb_write_byte(color[i + 1]);
        spi_bb_write_byte(color[i]);
    }
    gpio_put(SPI_CS_PIN, 1);

    lv_display_flush_ready(disp);  // tell LVGL we're done
}

static uint32_t my_tick_get_cb(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

static void my_delay_cb(uint32_t ms) {
    sleep_ms(ms);
}

void lv_example_get_started_1(void)
{
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello world");
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

int main(void) {
    stdio_init_all();

    // Initialize GPIO pins for bit-banging
    gpio_init(SPI_MOSI_PIN); gpio_set_dir(SPI_MOSI_PIN, GPIO_OUT); gpio_put(SPI_MOSI_PIN, 0);
    gpio_init(SPI_CLK_PIN);  gpio_set_dir(SPI_CLK_PIN, GPIO_OUT);  gpio_put(SPI_CLK_PIN, 0);
    gpio_init(SPI_CS_PIN);   gpio_set_dir(SPI_CS_PIN, GPIO_OUT);   gpio_put(SPI_CS_PIN, 1);
    gpio_init(SPI_DC_PIN);   gpio_set_dir(SPI_DC_PIN, GPIO_OUT);   gpio_put(SPI_DC_PIN, 1);
    gpio_init(SPI_RST_PIN);  gpio_set_dir(SPI_RST_PIN, GPIO_OUT);  gpio_put(SPI_RST_PIN, 1);
    gpio_init(SPI_BL_PIN);   gpio_set_dir(SPI_BL_PIN, GPIO_OUT);   gpio_put(SPI_BL_PIN, 1);

    // Hardware reset sequence
    gpio_put(SPI_RST_PIN, 1); sleep_ms(50);
    gpio_put(SPI_RST_PIN, 0); sleep_ms(200);
    gpio_put(SPI_RST_PIN, 1); sleep_ms(200);

    lv_init();
    lv_tick_set_cb(my_tick_get_cb);
    lv_delay_set_cb(my_delay_cb);

    // Create display with LVGL ST7796 driver
    // ST7796 panels are natively 320x480 (Portrait). We define it as such, then rotate it.
    lv_display_t *disp = lv_st7796_create(LCD_HEIGHT, LCD_WIDTH, LV_LCD_FLAG_NONE, send_cmd_cb, send_color_cb);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

    // Allocate draw buffers (1/10 of screen size)
    uint32_t buf_size = LCD_WIDTH * LCD_HEIGHT * 2 / 10;
    void * buf1 = malloc(buf_size);
    void * buf2 = malloc(buf_size);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_example_get_started_1();

    while (1) {
        lv_timer_handler();
        sleep_ms(5);
    }
}
