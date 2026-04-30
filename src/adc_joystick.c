#include "adc_joystick.h"
#include "config.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdio.h>

/**
 * ADC Joystick Interface - Reimplemented for I2C Adafruit 5743
 * Maintains compatible API but reads from I2C device instead of ADC
 */

#define REG_X_LSB 0x00
#define REG_X_MSB 0x01
#define REG_Y_LSB 0x02
#define REG_Y_MSB 0x03
#define I2C_READ_RETRIES 3

static volatile bool sw_pressed = false;
static volatile uint32_t last_press_ms = 0;

/**
 * Read 16-bit value from two consecutive I2C registers
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

void joystick_init(void)
{
    // Initialize I2C
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    printf("[JOYSTICK] I2C initialized on GPIO%d (SDA) and GPIO%d (SCL) at %dHz\n",
           I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
    printf("[JOYSTICK] Reading from Adafruit 5743 at address 0x%02X\n", JOYSTICK_I2C_ADDR);
}

JoystickReading joystick_read(void)
{
    JoystickReading r;
    r.x = i2c_read_16bit(REG_X_LSB);
    r.y = i2c_read_16bit(REG_Y_LSB);
    return r;
}

void joystick_sw_init(void)
{
    // Joystick select button is handled by button polling in gpio_buttons.c
    sw_pressed = false;
    last_press_ms = 0;
}

bool joystick_sw_consume(void)
{
    // Not used in I2C implementation - use button_get_state() instead
    return false;
}

void joystick_sw_isr(void)
{
    // Not used in I2C implementation
}
