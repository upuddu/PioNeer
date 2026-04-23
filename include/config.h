#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

#ifndef M_PI
#define M_PI 3.141592f
#endif

// ─── GPIO BUTTONS (XYAB) ──────────────────────────────────────────────────────
#define BTN_A_PIN 36 // check
#define BTN_B_PIN 37 // check
#define BTN_X_PIN 34 // check
#define BTN_Y_PIN 35 // check

// ─── JOYSTICK (ADC) ───────────────────────────────────────────────────────────
#define JOYSTICK_X_PIN 40 // ADC channel 0 check
#define JOYSTICK_Y_PIN 41 // ADC channel 1 check
#define JOYSTICK_X_CH 0
#define JOYSTICK_Y_CH 1
#define JOYSTICK_SW_PIN 39

// ─── TFT LCD (SPI0) ───────────────────────────────────────────────────────────
#define SPI_PORT spi0
#define SPI_CS_PIN 17                 // GPIO17_SPI0_CS check
#define SPI_DC_PIN 22                 // GPIO21 (DC/RS) check
#define SPI_RST_PIN 21                // GPIO22 (RST) check
#define SPI_MOSI_PIN 19               // GPIO19_SPI0_TX check
#define SPI_CLK_PIN 18                // GPIO18_SPI0_SCK check
#define SPI_MISO_PIN 16               // GPIO16_SPI0_RX check
#define SPI_BL_PIN 20                 // GPIO20 (LED/backlight)
#define SPI_BAUD_HZ (2 * 1000 * 1000) // drop to 2MHz for testing

// ─── AUDIO (PWM) ─────────────────────────────────────────────────────────────
#define AUDIO_PIN 38 // GP38 → PWM, through 4-stage filter to speaker check

// ─── WS2812B LED STRIP ────────────────────────────────────────────────────────
#define WS2812_PIN 9
#define WS2812_NUM_LEDS 10

// ─── DISPLAY DIMENSIONS ───────────────────────────────────────────────────────
#define LCD_WIDTH 480
#define LCD_HEIGHT 320

// ─── SD CARD (SPI1) ───────────────────────────────────────────────────────────
#define SD_SPI_PORT spi1
#define SD_CS_PIN 25   // GPIO25_SPI1_CS
#define SD_MOSI_PIN 27 // GPIO27_SPI1_TX
#define SD_MISO_PIN 24 // GPIO24_SPI1_RX
#define SD_CLK_PIN 26  // GPIO26_SPI1_SCK

// ─── GAME SELECT ──────────────────────────────────────────────────────────────
typedef enum
{
    GAME_LUNAR_LANDER = 0,
    GAME_CAR_RACING,
    GAME_TANK_BATTLE,
    GAME_COUNT
} GameID;

#endif // CONFIG_H
