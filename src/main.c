#include "config.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/st7796/lv_st7796.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>

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

    // Swap bytes for 16-bit color in-place (ST7796 expects MSB first, Pico is little-endian)
    for (uint32_t i = 0; i < color_len; i += 2) {
        uint8_t temp = color[i];
        color[i] = color[i + 1];
        color[i + 1] = temp;
    }

    spi_write_blocking(SPI_PORT, color, color_len);
    gpio_put(SPI_CS_PIN, 1);

    lv_display_flush_ready(disp);  // tell LVGL we're done
}

static uint32_t my_tick_get_cb(void) {
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

static void my_delay_cb(uint32_t ms) {
    sleep_ms(ms);
}

static void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target_obj(e);
    if(code == LV_EVENT_CLICKED) {
        static uint8_t cnt = 0;
        cnt++;

        printf("Button clicked! Count: %d\n", cnt);

        /*Get the first child of the button which is the label and change its text*/
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Button: %d", cnt);
    }
}

void lv_example_get_started_2(void)
{
    lv_obj_t * btn = lv_button_create(lv_screen_active());     /*Add a button the current screen*/
    lv_obj_set_pos(btn, 10, 10);                            /*Set its position*/
    lv_obj_set_size(btn, 120, 50);                          /*Set its size*/
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);           /*Assign a callback to the button*/

    lv_obj_t * label = lv_label_create(btn);          /*Add a label to the button*/
    lv_label_set_text(label, "Button");                     /*Set the labels text*/
    lv_obj_center(label);
}

int main(void) {
    stdio_init_all();

    // SPI hardware init
    spi_init(SPI_PORT, SPI_BAUD_HZ);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CLK_PIN,  GPIO_FUNC_SPI);
    // gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI); // Optional if MISO isn't used

    // Control pins as GPIO outputs
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
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

    // Allocate draw buffers (1/10 of screen size)
    uint32_t buf_size = LCD_WIDTH * LCD_HEIGHT * 2 / 10;
    void * buf1 = malloc(buf_size);
    void * buf2 = malloc(buf_size);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_example_get_started_2();

    while (1) {
        lv_timer_handler();
        sleep_ms(1);
    }
}
