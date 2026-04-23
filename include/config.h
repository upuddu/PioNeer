#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

// ─── SD Card SPI Configuration
// ────────────────────────────────────────────────
#define SD_SPI_PORT spi1
#define SD_CS_PIN 25
#define SD_MOSI_PIN 27
#define SD_MISO_PIN 24
#define SD_CLK_PIN 26
#define SD_SPI_BAUD (10 * 1000 * 1000)

#endif // CONFIG_H