#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

// I2S pins (PIO-based) — Adafruit 6309 breakout (TLV320DAC3100)
#define I2S_BCK_PIN   9   // Bit Clock
#define I2S_WSEL_PIN  10  // Word Select / LRCLK
#define I2S_DIN_PIN   11  // Data In (to DAC)

// DAC I2C address (shares bus with joystick on I2C1)
#define DAC_I2C_ADDR  0x18

// Sample rate
#define I2S_SAMPLE_RATE 44100

// Init: configures DAC registers over I2C, starts PIO I2S output
bool i2s_audio_init(void);

// Play a tone on both channels (blocking, duration in ms)
void i2s_play_tone(float freq_hz, uint32_t duration_ms);

// Play a short startup chime
void i2s_play_chime(void);

// Play a scale
void i2s_play_scale(void);

// Silence
void i2s_silence(void);

#endif // I2S_AUDIO_H
