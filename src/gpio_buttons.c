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
 * Poll button states and fire callbacks on changes
 */
static int64_t button_poll_callback(alarm_id_t id, void *user_data)
{
    uint8_t current_state = read_button_register();
    
    // Adafruit 5743 button mapping (active-low):
    // Bit 7: SELECT button
    // Bit 6: Button A
    // Bit 5: Button B
    // Bit 4: Button X
    // Bit 3: Button Y
    
    static const uint8_t button_bits[] = {6, 5, 4, 3};  // A, B, X, Y
    
    for (int i = 0; i < 4; i++)
    {
        uint8_t bit_mask = (1 << button_bits[i]);
        bool is_pressed = !(current_state & bit_mask);  // Active-low logic
        
        ButtonState new_state = is_pressed ? BTN_STATE_PRESSED : BTN_STATE_RELEASED;
        
        // Trigger callback on state change
        if (btn_state[i] != new_state && btn_state[i] != BTN_STATE_UNKNOWN)
        {
            if (btn_callback)
            {
                btn_callback((Button)i, new_state);
            }
        }
        
        btn_state[i] = new_state;
    }
    
    // Reschedule polling (every 50ms)
    return 50000;  // 50ms in microseconds
}

void buttons_init(void)
{
    printf("[BUTTONS] Initializing I2C button interface via Adafruit 5743\n");
    
    // Initial read to set current state
    uint8_t initial_state = read_button_register();
    last_button_state = initial_state;
    
    static const uint8_t button_bits[] = {6, 5, 4, 3};
    
    for (int i = 0; i < 4; i++)
    {
        uint8_t bit_mask = (1 << button_bits[i]);
        bool is_pressed = !(initial_state & bit_mask);
        btn_state[i] = is_pressed ? BTN_STATE_PRESSED : BTN_STATE_RELEASED;
    }
    
    // Start periodic polling
    add_alarm_in_ms(50, button_poll_callback, NULL, false);
    
    printf("[BUTTONS] I2C button polling started\n");
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

