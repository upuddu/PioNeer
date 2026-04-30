#include "adc_joystick.h"
#include "config.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define DEBOUNCE_MS 50
#define I2C_READ_RETRIES 3

static volatile bool sw_pressed = false;
static volatile uint32_t last_press_ms = 0;

// Adafruit 5743 register map
#define REG_X_LSB 0x00
#define REG_X_MSB 0x01
#define REG_Y_LSB 0x02
#define REG_Y_MSB 0x03
#define REG_BUTTONS 0x04

/**
 * Read a 16-bit value from two consecutive registers via I2C
 */
static uint16_t i2c_read_16bit(uint8_t reg_lsb)
{
    uint8_t buffer[2] = {0};
    
    for (int retry = 0; retry < I2C_READ_RETRIES; retry++)
    {
        int result = i2c_write_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &reg_lsb, 1, true);
        if (result == PICO_ERROR_GENERIC)
        {
            sleep_ms(10);
            continue;
        }
        
        result = i2c_read_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, buffer, 2, false);
        if (result == PICO_ERROR_GENERIC)
        {
            sleep_ms(10);
            continue;
        }
        break;
    }
    
    // Combine LSB (first byte) and MSB (second byte)
    return ((uint16_t)buffer[1] << 8) | buffer[0];
}

/**
 * Read button state from the Adafruit 5743
 */
static uint8_t i2c_read_buttons(void)
{
    uint8_t reg = REG_BUTTONS;
    uint8_t buttons = 0xFF; // 0xFF = all released (active low)
    
    for (int retry = 0; retry < I2C_READ_RETRIES; retry++)
    {
        int result = i2c_write_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &reg, 1, true);
        if (result == PICO_ERROR_GENERIC)
        {
            sleep_ms(10);
            continue;
        }
        
        result = i2c_read_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &buttons, 1, false);
        if (result == PICO_ERROR_GENERIC)
        {
            sleep_ms(10);
            continue;
        }
        break;
    }
    
    return buttons;
}

void joystick_init(void)
{
    // Initialize I2C
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    printf("[JOYSTICK] Initialized I2C on GPIO%d (SDA) and GPIO%d (SCL) at %dHz\n",
           I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
    printf("[JOYSTICK] Adafruit 5743 I2C address: 0x%02X\n", JOYSTICK_I2C_ADDR);
}

JoystickReading joystick_read(void)
{
    JoystickReading r;
    
    // Read X and Y analog values (16-bit each)
    r.x = i2c_read_16bit(REG_X_LSB);
    r.y = i2c_read_16bit(REG_Y_LSB);
    
    return r;
}

void joystick_sw_init(void)
{
    // No additional initialization needed for I2C-based button reading
    // Button state is read directly from the device
    sw_pressed = false;
    last_press_ms = 0;
}

bool joystick_sw_consume(void)
{
    // Read current button state
    uint8_t buttons = i2c_read_buttons();
    bool button_pressed = !(buttons & 0x80); // Check if select button (bit 7) is pressed
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (button_pressed && (now - last_press_ms > DEBOUNCE_MS))
    {
        last_press_ms = now;
        return true;
    }
    
    return false;
}

void joystick_sw_isr(void)
{
    // Not used for I2C implementation
    // I2C is polled, not interrupt-driven
}
