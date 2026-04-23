#include <stdio.h>
#include "gpio_buttons.h"
#include "config.h"
#include "hardware/irq.h"

static const uint BTN_PINS[] = {
    BTN_A_PIN, BTN_B_PIN, BTN_X_PIN, BTN_Y_PIN
};

static const char* BTN_NAMES[] = {
    "Button A (JoyA)", "Button B (JoyB)", "Button X (JoyX)", "Button Y (JoyY)"
};

void buttons_init(void) {
    for (int i = 0; i < 4; i++) {
        uint gpio = BTN_PINS[i];
        uint32_t mask = 1u << (gpio & 0x1fu);
        gpio_function_t fn = (gpio_function_t)5;  // SIO function

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

// GPIO interrupt handler — called when any button edge is detected
static void gpio_irq_handler(void) {
    for (int i = 0; i < 4; i++) {
        uint gpio = BTN_PINS[i];
        // Each GPIO has 4 event bits, in register [gpio/8], at bit offset (gpio%8)*4
        uint reg_idx = gpio / 8;
        uint bit_offset = (gpio % 8) * 4;

        // Check raw interrupt status for edge rise (bit 3 of the 4-bit group)
        uint32_t intr = io_bank0_hw->intr[reg_idx];
        if (intr & (0x8u << bit_offset)) {  // edge rise
            // Acknowledge the interrupt by writing 1 to the raw status bit
            io_bank0_hw->intr[reg_idx] = (0x8u << bit_offset);
            printf("Pressed: %s\n", BTN_NAMES[i]);
        }
    }
}

void buttons_enable_interrupts(void) {
    for (int i = 0; i < 4; i++) {
        uint gpio = BTN_PINS[i];
        uint reg_idx = gpio / 8;
        uint bit_offset = (gpio % 8) * 4;

        // Enable edge-rise interrupt (bit 3) for this GPIO on proc0
        io_bank0_hw->proc0_irq_ctrl.inte[reg_idx] |= (0x8u << bit_offset);
    }

    // Set our handler and enable the IO_IRQ_BANK0 interrupt
    irq_set_exclusive_handler(IO_IRQ_BANK0, gpio_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

bool button_is_pressed(Button btn) {
    uint gpio = BTN_PINS[btn];
    // GPIO34-37 are in the high bank (>=32), read from gpio_hi_in
    // Active-high: buttons have external 10k pull-down, switch pulls to +3V3
    return (sio_hw->gpio_hi_in & (1u << (gpio - 32))) ? true : false;
}
