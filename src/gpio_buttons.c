#include "gpio_buttons.h"
#include "config.h"

static const uint BTN_PINS[] = {
    BTN_A_PIN, BTN_B_PIN, BTN_X_PIN, BTN_Y_PIN
};

void buttons_init(void) {
    for (int i = 0; i < 4; i++) {
        uint gpio = BTN_PINS[i];
        uint32_t mask = 1u << (gpio & 0x1fu);
        gpio_function_t fn = 5;  // SIO function

        // Set as input (clear output enable)
        // GPIO34-37 are in the high bank (>=32), use gpio_hi registers
        sio_hw->gpio_hi_oe_clr = mask;

        // Enable input, disable output driver
        hw_write_masked(&pads_bank0_hw->io[gpio],
                   PADS_BANK0_GPIO0_IE_BITS,
                   PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);

        // Set function to SIO
        io_bank0_hw->io[gpio].ctrl = fn << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

        // Clear isolation bit
        hw_clear_bits(&pads_bank0_hw->io[gpio], PADS_BANK0_GPIO0_ISO_BITS);
    }
}

bool button_is_pressed(Button btn) {
    uint gpio = BTN_PINS[btn];
    // GPIO34-37 are in the high bank (>=32), read from gpio_hi_in
    // Active-high: buttons have external 10k pull-down, switch pulls to +3V3
    return (sio_hw->gpio_hi_in & (1u << (gpio - 32))) ? true : false;
}
