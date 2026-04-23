#include "gpio_buttons.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

static const uint BTN_PINS[] = {
    BTN_A_PIN, BTN_B_PIN, BTN_X_PIN, BTN_Y_PIN};

// History byte for debouncing: 0xFF = pressed, 0x00 = released
static uint8_t btn_history[4] = {0};
static ButtonState btn_state[4] = {BTN_STATE_UNKNOWN, BTN_STATE_UNKNOWN, BTN_STATE_UNKNOWN, BTN_STATE_UNKNOWN};
static button_callback_t btn_callback = NULL;

// Timer callback for debounce sampling
static int64_t debounce_callback(alarm_id_t id, void *user_data)
{
    for (int i = 0; i < 4; i++)
    {
        uint gpio = BTN_PINS[i];
        uint raw_state = (sio_hw->gpio_hi_in & (1u << (gpio - 32))) ? 1 : 0;

        // Shift history byte: 1 = not pressed (HIGH), 0 = pressed (LOW)
        // Active-high with pull-down: unpressed=HIGH(1), pressed=LOW(0)
        btn_history[i] = (btn_history[i] << 1) | raw_state;

        // Check for state change (all bits must agree)
        ButtonState new_state = BTN_STATE_UNKNOWN;
        if (btn_history[i] == 0x00)
        {
            // All zeros = all pressed (LOW) = PRESSED
            new_state = BTN_STATE_PRESSED;
        }
        else if (btn_history[i] == 0xFF)
        {
            // All ones = all unpressed (HIGH) = RELEASED
            new_state = BTN_STATE_RELEASED;
        }

        // Call callback on state change
        if (new_state != BTN_STATE_UNKNOWN && new_state != btn_state[i])
        {
            btn_state[i] = new_state;
            if (btn_callback)
            {
                btn_callback((Button)i, new_state);
            }
        }
    }

    // Reschedule debounce check (every 5ms)
    return 5000; // 5ms in microseconds
}

static void gpio_interrupt_handler(uint gpio, uint32_t events)
{
    if (gpio == JOYSTICK_SW_PIN)
    {
        joystick_sw_isr(); // delegate to joystick handler
    }
    // Interrupt triggered, debounce sampling will handle state detection
    // This just ensures we start sampling on any edge
}

void buttons_init(void)
{
    // Initialize GPIO inputs
    for (int i = 0; i < 4; i++)
    {
        uint gpio = BTN_PINS[i];
        uint32_t mask = 1u << (gpio & 0x1fu);

        // Set as input (clear output enable)
        sio_hw->gpio_hi_oe_clr = mask;

        // Enable input, disable output driver
        hw_write_masked(&pads_bank0_hw->io[gpio],
                        PADS_BANK0_GPIO0_IE_BITS,
                        PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);

        // Set function to SIO
        io_bank0_hw->io[gpio].ctrl = 5u << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

        // Clear isolation bit
        hw_clear_bits(&pads_bank0_hw->io[gpio], PADS_BANK0_GPIO0_ISO_BITS);
    }

    // Set up GPIO interrupt handler FIRST before enabling interrupts
    gpio_set_irq_callback(gpio_interrupt_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);

    // Now enable GPIO interrupt on all button pins (both edges for robust detection)
    for (int i = 0; i < 4; i++)
    {
        gpio_set_irq_enabled(BTN_PINS[i], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    }

    // Start debounce timer (5ms interval)
    add_alarm_in_ms(5, debounce_callback, NULL, false);
}

void buttons_set_callback(button_callback_t callback)
{
    btn_callback = callback;
}

ButtonState button_get_state(Button btn)
{
    return btn_state[btn];
}
