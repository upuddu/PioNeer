#ifndef SPI_DISPLAY_H
#define SPI_DISPLAY_H

#include <stdint.h>

// Basic colors (RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F

void display_init(void);
void display_clear(uint16_t color);
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void display_write_string(uint16_t x, uint16_t y, const char *str, uint16_t color);

#endif // SPI_DISPLAY_H
