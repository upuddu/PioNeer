#ifndef SPI_DISPLAY_H
#define SPI_DISPLAY_H

#include "lvgl.h"

// Basic colors (RGB565) - keeping these for compatibility if needed, though LVGL has its own colors
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F

void display_init(void);

#endif // SPI_DISPLAY_H
