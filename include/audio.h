#ifndef AUDIO_H
#define AUDIO_H

#include <math.h>
#include <stdint.h>

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

// PWM hardware
void init_pwm_audio(void);

// Test sequences
void play_melody(void);
void play_sweep(void);
void play_chord(void);

#endif