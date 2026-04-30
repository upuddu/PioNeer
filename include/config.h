#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

#ifndef M_PI
#define M_PI 3.141592f
#endif

// ─── I2C JOYSTICK (ADAFRUIT 5743 - STEMMA QT) ────────────────────────────────
#define I2C_PORT i2c1
#define I2C_SDA_PIN 2  // GPIO2
#define I2C_SCL_PIN 3  // GPIO3
#define I2C_FREQ 400000 // 400kHz standard I2C
#define JOYSTICK_I2C_ADDR 0x50 // Adafruit 5743 default I2C address

// ─── BUTTONS (From I2C Joystick) ──────────────────────────────────────────────
// Adafruit 5743 provides 4 buttons (A, B, X, Y) over I2C
// No GPIO pins needed, all data comes from I2C

// ─── JOYSTICK (From I2C) ──────────────────────────────────────────────────────
// X and Y axes read from Adafruit 5743 over I2C
// No ADC pins needed

// ─── HSTX DVI/HDMI DISPLAY (22-pin breakout) ──────────────────────────────────
// RP2350A has dedicated HSTX pins (GPIO0-23 reserved for video output)
// No explicit pin configuration needed - HSTX uses fixed pins
#define HSTX_ENABLED 1
#define LCD_WIDTH 640
#define LCD_HEIGHT 480

// ─── AUDIO (PWM) ─────────────────────────────────────────────────────────────
#define AUDIO_PIN 38 // GP38 → PWM, through 4-stage filter to speaker check

// ─── WS2812B LED STRIP ────────────────────────────────────────────────────────
#define WS2812_PIN 21
#define WS2812_NUM_LEDS 10

// ─── DISPLAY DIMENSIONS ───────────────────────────────────────────────────────
// Set above for HSTX (640x480)

// ─── SD CARD (SPI1) ───────────────────────────────────────────────────────────
#define SD_SPI_PORT spi1
#define SD_CS_PIN 25   // GPIO25_SPI1_CS
#define SD_MOSI_PIN 27 // GPIO27_SPI1_TX
#define SD_MISO_PIN 24 // GPIO24_SPI1_RX
#define SD_CLK_PIN 26  // GPIO26_SPI1_SCK
#define SD_SPI_BAUD (10 * 1000 * 1000) // BAUD Rate

// ─── GAME SELECT ──────────────────────────────────────────────────────────────
typedef enum
{
    GAME_LUNAR_LANDER = 0,
    GAME_CAR_RACING,
    GAME_TANK_BATTLE,
    GAME_COUNT
} GameID;

#endif // CONFIG_H
