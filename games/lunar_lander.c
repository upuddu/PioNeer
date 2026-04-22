#include "lunar_lander.h"
#include "adc_joystick.h"
#include "gpio_buttons.h"
#include "spi_display.h"
#include "pio_neopixel.h"
#include "i2c_dac.h"
#include "pico/stdlib.h"

// ── Game state ────────────────────────────────────────────────────────────────
typedef struct {
    float    x, y;          // lander position
    float    vx, vy;        // velocity
    uint8_t  fuel;          // 0–100 (maps to NeoPixel strip)
    bool     landed;
    bool     crashed;
} LanderState;

static LanderState state;

static void lander_reset(void) {
    state.x       = 120.0f;
    state.y       = 10.0f;
    state.vx      = 0.0f;
    state.vy      = 0.0f;
    state.fuel    = 100;
    state.landed  = false;
    state.crashed = false;
}

static void lander_update(JoystickReading joy) {
    if (state.fuel == 0) return;

    // Map joystick (0–4095, center ~2048) to thrust
    float thrust_x = (joy.x - 2048.0f) / 2048.0f * 0.05f;
    float thrust_y = (joy.y - 2048.0f) / 2048.0f * 0.05f;

    state.vx += thrust_x;
    state.vy += thrust_y + 0.01f;  // gravity
    state.x  += state.vx;
    state.y  += state.vy;
    state.fuel--;

    neopixel_set_resource(state.fuel);
    neopixel_show();
}

static void lander_draw(void) {
    display_clear(COLOR_BLACK);
    // TODO: draw terrain, lander sprite, HUD
}

void lunar_lander_run(void) {
    lander_reset();
    display_clear(COLOR_BLACK);

    while (!state.landed && !state.crashed) {
        JoystickReading joy = joystick_read();
        lander_update(joy);
        lander_draw();

        if (button_is_pressed(BTN_A)) {
            // TODO: fire retro rocket / extra thrust
        }

        sleep_ms(16);  // ~60 fps target
    }

    if (state.landed) {
        dac_play_tone(880, 300);  // success beep
        display_write_string(80, 150, "LANDED!", COLOR_GREEN);
    } else {
        dac_play_tone(220, 600);  // crash sound
        display_write_string(70, 150, "CRASHED!", COLOR_RED);
    }

    sleep_ms(2000);
}
