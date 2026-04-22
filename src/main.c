#include "pico/stdlib.h"
#include "config.h"
#include "adc_joystick.h"
#include "spi_display.h"
#include "i2c_dac.h"
#include "pio_neopixel.h"
#include "gpio_buttons.h"
#include "lunar_lander.h"
#include "car_racing.h"
#include "tank_battle.h"

int main(void) {
    stdio_init_all();

    // ── Init all peripherals ──────────────────────────────────────────────────
    buttons_init();
    joystick_init();
    display_init();
    dac_init();
    neopixel_init();

    // ── Game selection loop ───────────────────────────────────────────────────
    GameID selected = GAME_LUNAR_LANDER;  // default; replace with menu logic

    while (true) {
        switch (selected) {
            case GAME_LUNAR_LANDER: lunar_lander_run(); break;
            case GAME_CAR_RACING:   car_racing_run();   break;
            case GAME_TANK_BATTLE:  tank_battle_run();  break;
            default: break;
        }
    }

    return 0;
}
