#include "gpio_buttons.h"
#include "config.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdio.h>

/**
 * GPIO Buttons Interface Implementation using I2C Joystick (Adafruit 5743)
 * Maintains compatible API with original GPIO-based implementation
 */

// Button register state caching
static uint8_t last_button_state = 0xFF; // All released (active-low)
static ButtonState btn_state[4] = {BTN_STATE_UNKNOWN, BTN_STATE_UNKNOWN, BTN_STATE_UNKNOWN, BTN_STATE_UNKNOWN};
static button_callback_t btn_callback = NULL;

#define REG_BUTTONS 0x04  // Adafruit 5743 button register
#define I2C_READ_RETRIES 3

/**
 * Read button state from I2C joystick
 */
static uint8_t read_button_register(void)
{
    uint8_t reg = REG_BUTTONS;
    uint8_t buttons = 0xFF;  // Default: all released
    
    for (int retry = 0; retry < I2C_READ_RETRIES; retry++)
    {
        int result = i2c_write_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &reg, 1, true);
        if (result == PICO_ERROR_GENERIC)
        {
            sleep_ms(5);
            continue;
        }
        
        result = i2c_read_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &buttons, 1, false);
        if (result == PICO_ERROR_GENERIC)
        {
            sleep_ms(5);
            continue;
        }
        return buttons;
    }
    
    return buttons;
}

/**
 * Poll button states and fire callbacks on changes.
 * Call this from the main loop — NOT from any interrupt / alarm context.
 * Rate-limited internally to at most once every 50 ms so it doesn’t
 * flood the I2C bus between joystick reads.
 */
void buttons_poll(void)
{
    static uint32_t last_poll_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_poll_ms < 50) return;   // rate-limit: max 20 Hz
    last_poll_ms = now;

    uint8_t current_state = read_button_register();

    // Adafruit 5743 button mapping (active-low):
    // Bit 7: SELECT button
    // Bit 6: Button A  Bit 5: Button B  Bit 4: Button X  Bit 3: Button Y
    static const uint8_t button_bits[] = {6, 5, 4, 3};  // A, B, X, Y

    for (int i = 0; i < 4; i++)
    {
        uint8_t bit_mask = (1u << button_bits[i]);
        bool is_pressed = !(current_state & bit_mask);  // active-low

        ButtonState new_state = is_pressed ? BTN_STATE_PRESSED : BTN_STATE_RELEASED;

        if (btn_state[i] != new_state && btn_state[i] != BTN_STATE_UNKNOWN)
        {
            if (btn_callback)
                btn_callback((Button)i, new_state);
        }

        btn_state[i] = new_state;
    }
}

void buttons_init(void)
{
    printf("[BUTTONS] Initializing I2C button interface via Adafruit 5743\n");

    // Initial read to establish baseline state (safe — we're in thread context)
    uint8_t initial_state = read_button_register();
    last_button_state = initial_state;

    static const uint8_t button_bits[] = {6, 5, 4, 3};

    for (int i = 0; i < 4; i++)
    {
        uint8_t bit_mask = (1u << button_bits[i]);
        bool is_pressed = !(initial_state & bit_mask);
        btn_state[i] = is_pressed ? BTN_STATE_PRESSED : BTN_STATE_RELEASED;
    }

    // NOTE: No alarm is started here. Call buttons_poll() from your main loop.
    printf("[BUTTONS] Init done. Call buttons_poll() from main loop.\n");
}

void buttons_set_callback(button_callback_t callback)
{
    btn_callback = callback;
}

ButtonState button_get_state(Button btn)
{
    if (btn >= 4)
        return BTN_STATE_UNKNOWN;
    
    return btn_state[btn];
}

