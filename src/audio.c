#include "audio.h"
#include "config.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// Definitions
short int wavetable[N]; // moved here from header
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;
int volume = 2400;
WaveformType current_waveform = WAVE_SINE;

// ── Wavetable generators ───────────────────────────────────────────
void init_wavetable(void)
{
    for (int i = 0; i < N; i++)
        wavetable[i] = (16383 * sin(2 * M_PI * i / N)) + 16384;
}

void init_wavetable_sawtooth(void)
{
    for (int i = 0; i < N; i++)
        wavetable[i] = (32767 * i / N);
}

void init_wavetable_square(void)
{
    for (int i = 0; i < N; i++)
        wavetable[i] = (i < N / 2) ? 32767 : 0;
}

void init_wavetable_triangle(void)
{
    for (int i = 0; i < N; i++)
    {
        if (i < N / 2)
            wavetable[i] = (32767 * i / (N / 2));
        else
            wavetable[i] = (32767 * (N - i) / (N / 2));
    }
}

void set_waveform(WaveformType wave)
{
    current_waveform = wave;
    switch (wave)
    {
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

void set_freq(int chan, float f)
{
    if (chan == 0)
    {
        step0 = (f == 0.0f) ? 0 : (int)((f * N / RATE) * (1 << 16));
        if (f == 0.0f)
            offset0 = 0;
    }
    if (chan == 1)
    {
        step1 = (f == 0.0f) ? 0 : (int)((f * N / RATE) * (1 << 16));
        if (f == 0.0f)
            offset1 = 0;
    }
}

// ── PWM hardware ───────────────────────────────────────────────────
static void pwm_audio_handler(void)
{
    uint8_t pin = 38;
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel_num = pwm_gpio_to_channel(pin);

    pwm_hw->intr = 1u << slice_num;

    offset0 += step0;
    offset1 += step1;
    if (offset0 >= (N << 16))
        offset0 -= (N << 16);
    if (offset1 >= (N << 16))
        offset1 -= (N << 16);

    uint32_t samp = ((uint32_t)wavetable[offset0 >> 16] + (uint32_t)wavetable[offset1 >> 16]) / 2;
    samp = (samp * (pwm_hw->slice[slice_num].top + 1)) >> 16;

    uint32_t keep_mask = 0xffff << (16 * (!channel_num));
    pwm_hw->slice[slice_num].cc = (pwm_hw->slice[slice_num].cc & keep_mask) | (samp << (16 * channel_num));
}

void init_pwm_audio(void)
{
    uint8_t pin = 38;
    uint slice_num = pwm_gpio_to_slice_num(pin);

    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_hw->slice[slice_num].div = 150 << 4;
    pwm_hw->slice[slice_num].top = 1 * 1000000 / RATE - 1;
    pwm_hw->slice[slice_num].cc = 0;

    init_wavetable();

    pwm_hw->intr = 1u << slice_num;
    pwm_hw->inte |= 0x1 << slice_num;
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, pwm_audio_handler);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);
    pwm_hw->en |= 0x1 << slice_num;
}

// ── Test sequences ─────────────────────────────────────────────────
void play_melody(void)
{
    float notes[] = {262, 294, 330, 349, 392, 440, 494, 523, 494, 440, 392, 349, 330, 294, 262};
    for (int i = 0; i < 15; i++)
    {
        set_freq(0, notes[i]);
        sleep_ms(250);
    }
    set_freq(0, 0);
}

void play_sweep(void)
{
    for (float f = 100; f < 2000; f += 20)
    {
        set_freq(0, f);
        sleep_ms(10);
    }
    for (float f = 2000; f > 100; f -= 20)
    {
        set_freq(0, f);
        sleep_ms(10);
    }
    set_freq(0, 0);
}

void play_chord(void)
{
    set_freq(0, 262);
    set_freq(1, 330);
    sleep_ms(1000);
    set_freq(0, 0);
    set_freq(1, 0);
}