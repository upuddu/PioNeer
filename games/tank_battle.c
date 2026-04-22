#include "tank_battle.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "spi_display.h"
#include "pio_neopixel.h"
#include "i2c_dac.h"
#include "pico/stdlib.h"

// LED strip = ammo count

void tank_battle_run(void) {
    uint8_t ammo = 100;

    display_clear(COLOR_BLACK);
    neopixel_set_resource(ammo);
    neopixel_show();

    while (true) {
        JoystickReading joy = joystick_read();

        // TODO: move tank, aim turret, detect hits

        if (button_is_pressed(BTN_A) && ammo > 0) {
            ammo -= 10;                      // fire shell
            neopixel_set_resource(ammo);
            neopixel_show();
            dac_play_tone(300, 100);         // boom sound
        }

        sleep_ms(16);
    }
}
