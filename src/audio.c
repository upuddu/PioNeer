#include "audio.h"
#include <stdio.h>
#include "pico/stdlib.h"

int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;
int volume = 2400;

WaveformType current_waveform = WAVE_SINE;

void init_wavetable(void) {
    for(int i=0; i < N; i++)
        wavetable[i] = (16383 * sin(2 * M_PI * i / N)) + 16384;
}

void init_wavetable_sawtooth(void) {
    for(int i=0; i < N; i++)
        wavetable[i] = (32767 * i / N);
}

void init_wavetable_square(void) {
    for(int i=0; i < N; i++)
        wavetable[i] = (i < N/2) ? 32767 : 0;
}

void init_wavetable_triangle(void) {
    for(int i=0; i < N; i++) {
        if (i < N/2)
            wavetable[i] = (32767 * i / (N/2));
        else
            wavetable[i] = (32767 * (N - i) / (N/2));
    }
}

void set_waveform(WaveformType wave) {
    current_waveform = wave;
    switch (wave) {
        case WAVE_SINE:
            init_wavetable();
            printf("Switched to SINE\n");
            break;
        case WAVE_SAWTOOTH:
            init_wavetable_sawtooth();
            printf("Switched to SAWTOOTH\n");
            break;
        case WAVE_SQUARE:
            init_wavetable_square();
            printf("Switched to SQUARE\n");
            break;
        case WAVE_TRIANGLE:
            init_wavetable_triangle();
            printf("Switched to TRIANGLE\n");
            break;
    }
}

void set_freq(int chan, float f) {
    if (chan == 0) {
        if (f == 0.0) {
            step0 = 0;
            offset0 = 0;
        } else
            step0 = (f * N / RATE) * (1<<16);
    }
    if (chan == 1) {
        if (f == 0.0) {
            step1 = 0;
            offset1 = 0;
        } else
            step1 = (f * N / RATE) * (1<<16);
    }
}

// Simple melody: C major scale
void play_melody(void) {
    float notes[] = {262, 294, 330, 349, 392, 440, 494, 523, 494, 440, 392, 349, 330, 294, 262};
    int num_notes = 15;
    
    for (int i = 0; i < num_notes; i++) {
        set_freq(0, notes[i]);
        sleep_ms(250);
    }
    set_freq(0, 0);  // Stop
}

// Sweep test: ramps frequency up then down
void play_sweep(void) {
    printf("Playing frequency sweep...\n");
    // Sweep up
    for (float f = 100; f < 2000; f += 20) {
        set_freq(0, f);
        sleep_ms(10);
    }
    // Sweep down
    for (float f = 2000; f > 100; f -= 20) {
        set_freq(0, f);
        sleep_ms(10);
    }
    set_freq(0, 0);
}

// Chord test: plays 3 notes together
void play_chord(void) {
    printf("Playing C major chord (C-E-G)...\n");
    set_freq(0, 262);  // C
    set_freq(1, 330);  // E
    sleep_ms(1000);
    set_freq(0, 0);
    set_freq(1, 0);
}