#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

// ─── GPIO BUTTONS (XYAB) ──────────────────────────────────────────────────────
#define BTN_A_PIN       36
#define BTN_B_PIN       37
#define BTN_X_PIN       34
#define BTN_Y_PIN       35

// ─── WS2812B LED STRIP ────────────────────────────────────────────────────────
#define WS2812_PIN       9
#define WS2812_NUM_LEDS  20

#endif // CONFIG_H
