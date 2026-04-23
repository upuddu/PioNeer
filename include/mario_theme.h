#ifndef MARIO_THEME_H
#define MARIO_THEME_H

#include <stdint.h>

// Mario Theme Audio Data
// PCM Format: 16-bit signed little-endian, mono, 22050Hz sample rate
// Use with embedded flash storage or SD card playback

extern const uint16_t mario_theme_data[];
extern const uint32_t mario_theme_length;

#endif // MARIO_THEME_H
