#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "support.h"

//////////////////////////////////////////////////////////////////////////////

const char *username = "upuddu";

//////////////////////////////////////////////////////////////////////////////

void init_wavetable(void);
void set_freq(int chan, float f);
void set_waveform(WaveformType wave);
void play_melody(void);
void play_sweep(void);
void play_chord(void);

//////////////////////////////////////////////////////////////////////////////

// 8-bit audio synthesis
#define STEP4

//////////////////////////////////////////////////////////////////////////////

void pwm_audio_handler()
{
    uint8_t pin = 38;
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel_num = pwm_gpio_to_channel(pin);

    // Clear irq for WRAP0
    pwm_hw->intr = 1u << slice_num;

    offset0 += step0;
    offset1 += step1;

    if (offset0 >= (N << 16))
    {
        offset0 -= (N << 16);
    }

    if (offset1 >= (N << 16))
    {
        offset1 -= (N << 16);
    }

    // Use 32-bit precision to avoid overflow when mixing samples
    uint32_t samp = (uint32_t)wavetable[offset0 >> 16] + (uint32_t)wavetable[offset1 >> 16];
    samp /= 2;

    // Scale to PWM range with better precision
    samp = (samp * (pwm_hw->slice[slice_num].top + 1)) >> 16;

    uint32_t cur_cc = pwm_hw->slice[slice_num].cc;
    uint32_t keep_mask = 0xffff << (16 * (!channel_num));

    pwm_hw->slice[slice_num].cc = (cur_cc & keep_mask) | (samp << (16 * channel_num));

    return;
}

void init_pwm_audio()
{
    uint8_t pin = 38;
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Set pin as pwm output
    gpio_set_function(pin, GPIO_FUNC_PWM);
    // Set clock to 1/150 = 1 Mghz
    pwm_hw->slice[slice_num].div = 150 << 4;

    // Set output freq to 20 khz
    pwm_hw->slice[slice_num].top = 1 * 1000000 / RATE - 1;

    // Start from 0
    pwm_hw->slice[slice_num].cc = 0;

    /// Generate sin table
    init_wavetable();

    // Clear irq for WRAP0
    pwm_hw->intr = 1u << slice_num;

    // Enable int0 for slice num
    pwm_hw->inte |= 0x1 << slice_num;

    // Set exclusive handler for WRAP_0 PWM
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, pwm_audio_handler);

    // Enable WRAP_0 PWM interrupt
    irq_set_enabled(PWM_IRQ_WRAP_0, true);

    // Enable slice
    pwm_hw->en |= 0x1 << slice_num;

    return;
}

//////////////////////////////////////////////////////////////////////////////

int main()
{
    char freq_buf[9] = {0};
    int pos = 0;
    bool decimal_entered = false;
    int decimal_pos = 0;
    int current_channel = 0;

    keypad_init_pins();
    keypad_init_timer();
    display_init_pins();
    display_init_timer();

    init_pwm_audio();

    printf("\n=== PWM Audio Filter Test ===\n");
    printf("Playing test patterns...\n\n");

    // Test 1: Sine wave melody
    printf("1. SINE WAVE - C Major Scale\n");
    set_waveform(WAVE_SINE);
    play_melody();
    sleep_ms(500);

    // Test 2: Sawtooth sweep (great for hearing PWM artifacts)
    printf("2. SAWTOOTH - Frequency Sweep (0.1kHz to 2kHz)\n");
    set_waveform(WAVE_SAWTOOTH);
    play_sweep();
    sleep_ms(500);

    // Test 3: Square wave melody
    printf("3. SQUARE WAVE - C Major Scale\n");
    set_waveform(WAVE_SQUARE);
    play_melody();
    sleep_ms(500);

    // Test 4: Triangle wave melody
    printf("4. TRIANGLE WAVE - C Major Scale\n");
    set_waveform(WAVE_TRIANGLE);
    play_melody();
    sleep_ms(500);

    // Test 5: Chord (tests polyphony and harmonics)
    printf("5. SINE WAVE - C Major Chord\n");
    set_waveform(WAVE_SINE);
    play_chord();
    sleep_ms(500);

    // Test 6: Sawtooth chord (harsh - good for revealing filter response)
    printf("6. SAWTOOTH - C Major Chord\n");
    set_waveform(WAVE_SAWTOOTH);
    play_chord();
    sleep_ms(500);

    printf("\nTest sequence complete!\n");

    // Continuous mode - cycle through waveforms
    while(1)
    {
        printf("\n--- Cycling waveforms every 10 seconds ---\n");
        
        set_waveform(WAVE_SINE);
        set_freq(0, 440);
        sleep_ms(3000);
        
        set_waveform(WAVE_SAWTOOTH);
        sleep_ms(3000);
        
        set_waveform(WAVE_SQUARE);
        sleep_ms(3000);
        
        set_waveform(WAVE_TRIANGLE);
        sleep_ms(3000);
        
        set_freq(0, 0);
        sleep_ms(1000);
    }

    for (;;)
        ;
    return 0;
}
