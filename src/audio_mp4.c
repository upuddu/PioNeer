#include "audio_mp4.h"
#include "mario_theme.h"
#include "audio.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// ─── Audio Playback State ────────────────────────────────────────────────
typedef struct {
    const uint16_t *data_buffer;  // Pointer to audio data (flash or SD card)
    uint32_t buffer_length;       // Total samples in buffer
    uint32_t current_sample;      // Current playback position
    MP4PlayerState state;
    bool loop;
    float volume;
    bool from_flash;              // true=flash, false=SD card
} AudioPlaybackContext;

static AudioPlaybackContext audio_ctx = {0};
static int pwm_slice_num = 0;
static volatile uint32_t sample_counter = 0;

// ─── PWM Audio Configuration ────────────────────────────────────────────
#define AUDIO_PWM_FREQ 22050        // Match audio sample rate
#define PWM_CLOCK_DIV 4.0f          // Clock divider for PWM
#define PWM_WRAP 2840               // Wrap value for 22050Hz @ 125MHz sysclock

// ─── MP4 Player Initialization ──────────────────────────────────────────
void mp4_player_init(void)
{
    printf("[AUDIO] Initializing MP4 player with embedded flash audio\n");
    printf("[AUDIO] Audio format: 16-bit PCM, 22050Hz, mono\n");
    
    // Initialize PWM for audio output
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, PWM_CLOCK_DIV);
    pwm_config_set_wrap(&config, PWM_WRAP);
    pwm_init(pwm_slice_num, &config, true);
    pwm_set_gpio_level(AUDIO_PIN, 0);
    
    audio_ctx.state = MP4_STATE_STOPPED;
    audio_ctx.volume = 1.0f;
    audio_ctx.loop = false;
    audio_ctx.from_flash = true;
    
    printf("[AUDIO] PWM configured on GPIO%d\n", AUDIO_PIN);
    printf("[AUDIO] Ready to play audio\n");
}

// ─── Play from Embedded Flash ──────────────────────────────────────────
void mp4_play_embedded(void)
{
    printf("[AUDIO] ♫ Playing Mario theme from flash memory\n");
    
    audio_ctx.data_buffer = mario_theme_data;
    audio_ctx.buffer_length = mario_theme_length;
    audio_ctx.current_sample = 0;
    audio_ctx.state = MP4_STATE_PLAYING;
    audio_ctx.from_flash = true;
    
    printf("[AUDIO] Samples: %lu (%lu ms)\n", 
           audio_ctx.buffer_length, 
           (audio_ctx.buffer_length * 1000) / AUDIO_PWM_FREQ);
}

// ─── Play MP4 File (stub for future SD card support) ────────────────────
// For now: plays embedded mario_theme. Later: load from /audio/mario_theme.raw on SD
void mp4_play(const char *filename, bool loop)
{
    printf("[AUDIO] mp4_play() called: %s (loop=%d)\n", filename, loop);
    
    // For testing: always play embedded Mario theme
    // Later: Parse filename and load from SD card
    if (strstr(filename, "mario") != NULL || strstr(filename, "theme") != NULL)
    {
        audio_ctx.loop = loop;
        mp4_play_embedded();
    }
    else
    {
        printf("[AUDIO] Unknown file: %s\n", filename);
        printf("[AUDIO] Currently only mario_theme is embedded. Use mp4_play_embedded()\n");
    }
}

// ─── Pause Playback ─────────────────────────────────────────────────────
void mp4_pause(void)
{
    audio_ctx.state = MP4_STATE_PAUSED;
    pwm_set_gpio_level(AUDIO_PIN, 0);
    printf("[AUDIO] Paused at sample %lu\n", audio_ctx.current_sample);
}

// ─── Stop Playback ──────────────────────────────────────────────────────
void mp4_stop(void)
{
    audio_ctx.state = MP4_STATE_STOPPED;
    audio_ctx.current_sample = 0;
    pwm_set_gpio_level(AUDIO_PIN, 0);
    printf("[AUDIO] Stopped\n");
}

// ─── Set Volume (0.0 to 1.0) ────────────────────────────────────────────
void mp4_set_volume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    audio_ctx.volume = volume;
    printf("[AUDIO] Volume: %.1f%%\n", volume * 100.0f);
}

// ─── Get Current State ──────────────────────────────────────────────────
MP4PlayerState mp4_get_state(void)
{
    return audio_ctx.state;
}

// ─── Get Current Position (in samples) ──────────────────────────────────
uint32_t mp4_get_current_position(void)
{
    return audio_ctx.current_sample;
}

// ─── Get Total Duration (in samples) ────────────────────────────────────
uint32_t mp4_get_total_duration(void)
{
    return audio_ctx.buffer_length;
}

// ─── Update Function (call regularly from main loop) ────────────────────
// Streams audio samples to PWM output
void mp4_update(void)
{
    if (audio_ctx.state != MP4_STATE_PLAYING || !audio_ctx.data_buffer)
        return;

    // Get current audio sample (16-bit)
    uint16_t sample = audio_ctx.data_buffer[audio_ctx.current_sample];
    
    // Scale to PWM output range (0-1 normalized, map to 0-PWM_WRAP)
    // Input: 16-bit unsigned (0-65535)
    // Output: 0 to PWM_WRAP
    uint16_t pwm_level = (sample * audio_ctx.volume) / 65535.0f * PWM_WRAP;
    
    // Set PWM duty cycle
    pwm_set_gpio_level(AUDIO_PIN, pwm_level);
    
    // Advance to next sample
    audio_ctx.current_sample++;
    sample_counter++;
    
    // Check if reached end of audio
    if (audio_ctx.current_sample >= audio_ctx.buffer_length)
    {
        if (audio_ctx.loop)
        {
            // Reset to beginning for looping
            audio_ctx.current_sample = 0;
        }
        else
        {
            // Stop playing
            audio_ctx.state = MP4_STATE_STOPPED;
            pwm_set_gpio_level(AUDIO_PIN, 0);
            printf("[AUDIO] Playback finished (%lu samples)\n", sample_counter);
        }
    }
}

// ─── Cleanup ────────────────────────────────────────────────────────────
void mp4_player_cleanup(void)
{
    mp4_stop();
    printf("[AUDIO] MP4 player cleaned up\n");
}

// ─── IMPLEMENTATION NOTES ───────────────────────────────────────────────
//
// CURRENT (Testing with Flash):
//   - Mario theme embedded as uint16_t array in mario_theme.c
//   - Plays directly from flash memory
//   - No SD card required for testing
//   - Size: ~100 samples (4.5ms) placeholder, expandable to minutes
//
// FOR PRODUCTION (Load from SD Card):
//   1. Replace mp4_play() to read from /audio/mario_theme.raw on SD
//   2. Use SPI1 (already configured) for SD card I/O
//   3. Stream samples into a ring buffer
//   4. mp4_update() pulls from buffer instead of mario_theme_data[]
//   5. Reduces flash usage, allows larger audio files
//
// FUTURE IMPROVEMENTS:
//   - Add ADPCM or other compression for longer audio
//   - Implement ring buffer for SD streaming
//   - Add volume ramping for smooth transitions
//   - Support multiple audio sources (music + SFX mixing)

