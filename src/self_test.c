#include "self_test.h"
#include "led_strip.h"
#include "audio.h"
#include "config.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/st7796/lv_st7796.h"
#include <stdlib.h>

void led_self_test(void)
{
    printf("[LED] Starting self-test...\n");

    led_strip_set_all_color(COLOR_RED);
    led_strip_show();
    sleep_ms(400);
    led_strip_set_all_color(COLOR_GREEN);
    led_strip_show();
    sleep_ms(400);
    led_strip_set_all_color(COLOR_BLUE);
    led_strip_show();
    sleep_ms(400);

    led_strip_clear();
    for (uint8_t i = 0; i < WS2812_NUM_LEDS; i++)
    {
        if ((i % 3) == 0)
            led_strip_set_pixel(i, 255, 0, 0);
        else if ((i % 3) == 1)
            led_strip_set_pixel(i, 0, 255, 0);
        else
            led_strip_set_pixel(i, 0, 0, 255);
    }
    led_strip_show();
    sleep_ms(800);

    for (int b = 255; b >= 0; b -= 10)
    {
        led_strip_set_brightness((uint8_t)b);
        led_strip_show();
        sleep_ms(20);
    }
    for (int b = 0; b <= 255; b += 10)
    {
        led_strip_set_brightness((uint8_t)b);
        led_strip_show();
        sleep_ms(20);
    }

    led_strip_set_brightness(128);
    led_strip_rainbow_cycle(10);
    led_strip_rainbow_loading(100);

    led_strip_clear();
    led_strip_set_pixel(0, 255, 0, 0);
    led_strip_set_pixel(WS2812_NUM_LEDS - 1, 0, 255, 0);
    led_strip_show();
    sleep_ms(1000);

    led_strip_wave(COLOR_BLUE, 3, 50);
    led_strip_bounce(COLOR_YELLOW, 100);
    led_strip_pulse_pixel(2, COLOR_MAGENTA, 500);
    led_strip_blink_pixel(5, COLOR_CYAN, 200, 200, 3);
    led_strip_random_fill();
    led_strip_show();
    sleep_ms(2000);

    led_strip_clear();
    led_strip_show();
    printf("[LED] Self-test complete.\n");
}

void audio_self_test(void)
{
    printf("[AUDIO] Starting self-test...\n");

    printf("[AUDIO] 1. SINE - C Major Scale\n");
    set_waveform(WAVE_SINE);
    play_melody();
    sleep_ms(500);

    printf("[AUDIO] 2. SAWTOOTH - Frequency Sweep\n");
    set_waveform(WAVE_SAWTOOTH);
    play_sweep();
    sleep_ms(500);

    printf("[AUDIO] 3. SQUARE - C Major Scale\n");
    set_waveform(WAVE_SQUARE);
    play_melody();
    sleep_ms(500);

    printf("[AUDIO] 4. TRIANGLE - C Major Scale\n");
    set_waveform(WAVE_TRIANGLE);
    play_melody();
    sleep_ms(500);

    printf("[AUDIO] 5. SINE - C Major Chord\n");
    set_waveform(WAVE_SINE);
    play_chord();
    sleep_ms(500);

    printf("[AUDIO] 6. SAWTOOTH - C Major Chord\n");
    set_waveform(WAVE_SAWTOOTH);
    play_chord();

    set_freq(0, 0);
    set_freq(1, 0);
    printf("[AUDIO] Self-test complete.\n");
}

// static 콜백들 + display_self_test 함수
static void send_cmd_cb(lv_display_t *disp,
                        const uint8_t *cmd, uint32_t cmd_len,
                        const uint8_t *param, uint32_t param_len)
{
    gpio_put(SPI_DC_PIN, 0);
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_len);
    if (param && param_len)
    {
        gpio_put(SPI_DC_PIN, 1);
        spi_write_blocking(SPI_PORT, param, param_len);
    }
    gpio_put(SPI_CS_PIN, 1);
}

static void send_color_cb(lv_display_t *disp,
                          const uint8_t *cmd, uint32_t cmd_len,
                          uint8_t *color, uint32_t color_len)
{
    gpio_put(SPI_DC_PIN, 0);
    gpio_put(SPI_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, cmd, cmd_len);
    gpio_put(SPI_DC_PIN, 1);
    for (uint32_t i = 0; i < color_len; i += 2)
    {
        uint8_t temp = color[i];
        color[i] = color[i + 1];
        color[i + 1] = temp;
    }
    spi_write_blocking(SPI_PORT, color, color_len);
    gpio_put(SPI_CS_PIN, 1);
    lv_display_flush_ready(disp);
}

static uint32_t my_tick_get_cb(void)
{
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

static void my_delay_cb(uint32_t ms)
{
    sleep_ms(ms);
}

void display_self_test(void)
{
    printf("[DISPLAY] Starting self-test...\n");

    spi_init(SPI_PORT, SPI_BAUD_HZ);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CLK_PIN, GPIO_FUNC_SPI);

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

    gpio_put(SPI_RST_PIN, 1);
    sleep_ms(50);
    gpio_put(SPI_RST_PIN, 0);
    sleep_ms(200);
    gpio_put(SPI_RST_PIN, 1);
    sleep_ms(200);

    lv_init();
    lv_tick_set_cb(my_tick_get_cb);
    lv_delay_set_cb(my_delay_cb);

    lv_display_t *disp = lv_st7796_create(LCD_HEIGHT, LCD_WIDTH, LV_LCD_FLAG_NONE, send_cmd_cb, send_color_cb);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

    uint32_t buf_size = LCD_WIDTH * LCD_HEIGHT * 2 / 10;
    void *buf1 = malloc(buf_size);
    void *buf2 = malloc(buf_size);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 버튼 테스트 UI
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_pos(btn, 10, 10);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Display OK");
    lv_obj_center(label);

    // LVGL 몇 프레임 렌더링
    for (int i = 0; i < 100; i++)
    {
        lv_timer_handler();
        sleep_ms(10);
    }

    printf("[DISPLAY] Self-test complete.\n");
}
