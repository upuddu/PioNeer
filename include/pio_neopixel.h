#ifndef PIO_NEOPIXEL_H
#define PIO_NEOPIXEL_H

#include <stdint.h>

void neopixel_init(void);

// Set a single pixel (0-indexed). r/g/b: 0–255
void neopixel_set(uint index, uint8_t r, uint8_t g, uint8_t b);

// Push all pixel data to the strip
void neopixel_show(void);

// Set all pixels to one color
void neopixel_fill(uint8_t r, uint8_t g, uint8_t b);

// Clear all pixels (off)
void neopixel_clear(void);

// Set strip to show a resource level (0–100 %)
void neopixel_set_resource(uint8_t percent);

#endif // PIO_NEOPIXEL_H
