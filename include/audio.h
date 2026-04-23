#ifndef AUDIO_H
#define AUDIO_H

#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#define N 2000
#define RATE 80000

extern short int wavetable[N]; // defined in audio.c, not here

extern int step0;
extern int offset0;
extern int step1;
extern int offset1;
extern int volume;

typedef enum
{
    WAVE_SINE = 0,
    WAVE_SAWTOOTH = 1,
    WAVE_SQUARE = 2,
    WAVE_TRIANGLE = 3
} WaveformType;

extern WaveformType current_waveform;

// Wavetable init
void init_wavetable(void);
void init_wavetable_sawtooth(void);
void init_wavetable_square(void);
void init_wavetable_triangle(void);

// Audio control
void set_waveform(WaveformType wave);
void set_freq(int chan, float f);

// PCM sample playback (non-blocking). Plays `data` through the same PWM output.
// Samples are 8-bit unsigned (128 = silence). While a sample is playing,
// the wavetable oscillators are muted so the sample is not corrupted.
// Calling while another sample is playing replaces it immediately.
void play_sample(const uint8_t *data, uint32_t length, uint32_t sample_rate);

// Returns true while a sample is still being streamed to the PWM.
bool sample_is_playing(void);

// PWM hardware
void init_pwm_audio(void);

// Test sequences
void play_melody(void);
void play_sweep(void);
void play_chord(void);

#endif