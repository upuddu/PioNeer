#include "i2c_dac.h"
#include "config.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <math.h>

void dac_init(void) {
    i2c_init(I2C_PORT, I2C_BAUD_HZ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

void dac_write(uint16_t value) {
    // AD5693R: 3-byte write — command byte + 16-bit data (MSB first)
    uint8_t buf[3];
    buf[0] = 0x18;              // Write & update DAC register
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = value & 0xFF;
    i2c_write_blocking(I2C_PORT, DAC_ADDR, buf, 3, false);
}

void dac_play_tone(uint16_t freq_hz, uint32_t duration_ms) {
    // Simple software tone: toggle DAC between high/low at freq_hz
    uint32_t period_us = 1000000 / freq_hz;
    uint32_t half_us   = period_us / 2;
    uint32_t end_us    = duration_ms * 1000;
    uint32_t elapsed   = 0;

    while (elapsed < end_us) {
        dac_write(0xFFFF);
        sleep_us(half_us);
        dac_write(0x0000);
        sleep_us(half_us);
        elapsed += period_us;
    }
}
