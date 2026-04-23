#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "config.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "led_strip.h"
#include "audio.h"
#include "self_test.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/st7796/lv_st7796.h"
#include <stdlib.h>
#include "spi_display.h"
#include "car_game.h"

static const char *button_names[] = {"BTN_A", "BTN_B", "BTN_X", "BTN_Y"};

void button_event_handler(Button btn, ButtonState state)
{
    const char *state_str = (state == BTN_STATE_PRESSED) ? "pressed" : "released";
    printf("[BUTTONS] %s %s\n", button_names[btn], state_str);
    
    // Pass to Car Game
    car_button_callback((uint8_t)btn, (state == BTN_STATE_PRESSED));
}

static void joystick_poll(void)
{
    JoystickReading joy = joystick_read();
    int dx = (int)joy.x - 2048;
    int dy = (int)joy.y - 2048;

    if (dx > -100 && dx < 100 && dy > -100 && dy < 100)
    {
        printf("[JOY] CENTER\n");
    }
    else
    {
        float angle = atan2f((float)dy, (float)dx) * 180.0f / M_PI;
        if (angle < 0)
            angle += 360.0f;
        printf("[JOY] Angle: %.1f deg\n", angle);
    }

    if (joystick_sw_consume())
        printf("[JOY] CLICK\n");
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);
    printf("=== BOOT ===\n");
    printf("=== PioNeer System Test ===\n");

    printf("[INIT] Buttons...\n");
    buttons_init();
    buttons_set_callback(button_event_handler);

    printf("[INIT] Joystick...\n");
    joystick_init();
    joystick_sw_init();

    printf("[INIT] LED strip...\n");
    led_strip_init();

    printf("[INIT] Audio...\n");
    init_pwm_audio();

    printf("[INIT] Display & LVGL...\n");
    display_init();

    printf("\n=== CAR GAME START ===\n");
    car_game_init();

    printf("[MAIN] Entering main loop.\n");
    while (true)
    {
        joystick_poll();
        car_game_update();
        lv_timer_handler(); // Necessary for LVGL drawing to execute
        sleep_ms(20);       // Faster render loop for smoother gameplay!
    }
    return 0;
}