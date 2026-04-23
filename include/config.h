#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

/*
TODO: THE PIN NUMBERS NEED TO BE UPDATED.
    THEY ARE ALL CURRENTLY PLACEHOLDERS.
*/

// ─── TFT LCD (SPI0) ───────────────────────────────────────────────────────────
#define SPI_PORT        spi0
#define SPI_CS_PIN      17      // GPIO17_SPI0_CS check
#define SPI_DC_PIN      22      // GPIO21 (DC/RS) check
#define SPI_RST_PIN     21      // GPIO22 (RST) check 
#define SPI_MOSI_PIN    19      // GPIO19_SPI0_TX check
#define SPI_CLK_PIN     18      // GPIO18_SPI0_SCK check
#define SPI_MISO_PIN    16      // GPIO16_SPI0_RX check 
#define SPI_BL_PIN      20      // GPIO20 (LED/backlight)
#define SPI_BAUD_HZ     (2 * 1000 * 1000)   // drop to 2MHz for testing
#define LCD_WIDTH       480
#define LCD_HEIGHT      320

#endif