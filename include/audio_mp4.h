#ifndef AUDIO_MP4_H
#define AUDIO_MP4_H

#include <stdint.h>
#include <stdbool.h>

// ─── MP4 Audio Settings ──────────────────────────────────────────────────
#define MP4_BUFFER_SIZE 4096
#define MP4_SAMPLE_RATE 44100

typedef enum {
    MP4_STATE_STOPPED = 0,
    MP4_STATE_PLAYING = 1,
    MP4_STATE_PAUSED = 2
} MP4PlayerState;

// ─── MP4 Player Structure ────────────────────────────────────────────────
typedef struct {
    MP4PlayerState state;
    bool loop;
    float volume;
    uint32_t current_position;
    uint32_t total_duration;
} MP4Player;

// ─── Function Declarations ──────────────────────────────────────────────
// Initialization
void mp4_player_init(void);

// Playback control - Standard API
void mp4_play(const char *filename, bool loop);
void mp4_pause(void);
void mp4_stop(void);
void mp4_set_volume(float volume);

// Flash/Embedded Audio (for testing without SD card)
void mp4_play_embedded(void);

// Status queries
MP4PlayerState mp4_get_state(void);
uint32_t mp4_get_current_position(void);
uint32_t mp4_get_total_duration(void);

// Update (call regularly from main loop, ~22050 Hz for audio)
void mp4_update(void);

// Cleanup
void mp4_player_cleanup(void);

#endif // AUDIO_MP4_H
