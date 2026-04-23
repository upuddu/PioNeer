#ifndef SUPPORT_H
#define SUPPORT_H

#include <math.h>
#include <stdint.h>

// we defined this in main.c
#define N 2000 // Size of the wavetable (doubled for smoother waveforms)
short int wavetable[N];

#define RATE 80000  // 80 kHz sample rate (Nyquist = 40 kHz)

// defined as extern here so that we can share it between
// support.c and main.c, where they are included.
extern int step0;
extern int offset0;
extern int step1;
extern int offset1;

// Part 3: Analog-to-digital conversion for a volume level.
extern int volume;

// Waveform types for testing
typedef enum {
    WAVE_SINE = 0,
    WAVE_SAWTOOTH = 1,
    WAVE_SQUARE = 2,
    WAVE_TRIANGLE = 3
} WaveformType;

extern WaveformType current_waveform;

void init_wavetable(void);
void init_wavetable_sawtooth(void);
void init_wavetable_square(void);
void init_wavetable_triangle(void);
void set_waveform(WaveformType wave);
void set_freq(int chan, float f);
void play_melody(void);

#endif