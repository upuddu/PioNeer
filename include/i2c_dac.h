#ifndef I2C_DAC_H
#define I2C_DAC_H

#include <stdint.h>

void dac_init(void);
void dac_write(uint16_t value);       // 0–65535 (16-bit)
void dac_play_tone(uint16_t freq_hz, uint32_t duration_ms);

#endif // I2C_DAC_H
