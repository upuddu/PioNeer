#include "car_racing.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "spi_display.h"
#include "pio_neopixel.h"
#include "i2c_dac.h"
#include "pico/stdlib.h"

// LED strip = nitro charge level

void car_racing_run(void) {
    uint8_t nitro = 100;

    display_clear(COLOR_BLACK);
    neopixel_set_resource(nitro);
    neopixel_show();

    while (true) {
        JoystickReading joy = joystick_read();

        // TODO: move car, scroll road, detect collisions

        if (button_is_pressed(BTN_A) && nitro > 0) {
            nitro -= 5;                      // consume nitro
            neopixel_set_resource(nitro);
            neopixel_show();
            dac_play_tone(1200, 50);         // nitro sound
        }

        sleep_ms(16);
    }
}
