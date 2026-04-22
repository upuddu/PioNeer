#include "gpio_buttons.h"
#include "config.h"
#include "hardware/gpio.h"

static const uint BTN_PINS[] = {
    BTN_A_PIN, BTN_B_PIN, BTN_X_PIN, BTN_Y_PIN
};

void buttons_init(void) {
    for (int i = 0; i < 4; i++) {
        gpio_init(BTN_PINS[i]);
        gpio_set_dir(BTN_PINS[i], GPIO_IN);
        gpio_pull_up(BTN_PINS[i]);
    }
}

bool button_is_pressed(Button btn) {
    // Active-low with pull-up
    return !gpio_get(BTN_PINS[btn]);
}
