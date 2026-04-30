#ifndef SPI_DISPLAY_H
#define SPI_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

void display_init(void);
bool display_is_ready(void);

// Framebuffer access (RGB332)
uint8_t *display_get_framebuf(void);
void display_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void display_fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void display_clear(uint8_t r, uint8_t g, uint8_t b);

#endif // SPI_DISPLAY_H
