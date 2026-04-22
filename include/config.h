#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

/*
TODO: THE PIN NUMBERS NEED TO BE UPDATED.
    THEY ARE ALL CURRENTLY PLACEHOLDERS.
*/

// ─── GPIO BUTTONS (XYAB) ──────────────────────────────────────────────────────
#define BTN_A_PIN       10       
#define BTN_B_PIN       11
#define BTN_X_PIN       12
#define BTN_Y_PIN       13

// ─── JOYSTICK (ADC) ───────────────────────────────────────────────────────────
#define JOYSTICK_X_PIN  26      // ADC channel 0
#define JOYSTICK_Y_PIN  27      // ADC channel 1
#define JOYSTICK_X_CH   0
#define JOYSTICK_Y_CH   1

// ─── TFT LCD (SPI0) ───────────────────────────────────────────────────────────
#define SPI_PORT        spi0
#define SPI_CLK_PIN     18
#define SPI_MOSI_PIN    19
#define SPI_MISO_PIN    16
#define SPI_CS_PIN      17
#define SPI_DC_PIN      20
#define SPI_RST_PIN     21
#define SPI_BAUD_HZ     (10 * 1000 * 1000)  // 10 MHz

// ─── DAC (I2C) ────────────────────────────────────────────────────────────────
#define I2C_PORT        i2c0
#define I2C_SDA_PIN     4
#define I2C_SCL_PIN     5
#define I2C_BAUD_HZ     (400 * 1000)        // 400 kHz fast mode
#define DAC_ADDR        0x4C                // AD5693R default I2C address

// ─── NEOPIXEL (PIO) ───────────────────────────────────────────────────────────
#define NEOPIXEL_PIN    22
#define NEOPIXEL_COUNT  30                  // adjust to your strip length

// ─── DISPLAY DIMENSIONS ───────────────────────────────────────────────────────
#define LCD_WIDTH       240
#define LCD_HEIGHT      320

// ─── GAME SELECT ──────────────────────────────────────────────────────────────
typedef enum {
    GAME_LUNAR_LANDER = 0,
    GAME_CAR_RACING,
    GAME_TANK_BATTLE,
    GAME_COUNT
} GameID;

#endif // CONFIG_H
