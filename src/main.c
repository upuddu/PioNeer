/**
 * PioNeer Full Peripheral Test
 *
 * Tests all four peripheral subsystems in sequence:
 *   [1] HSTX DVI 22-pin display (640x480@60Hz)
 *   [2] I2C Joystick + Buttons (Adafruit 5743 @ I2C1)
 *   [3] I2S Audio (TLV320DAC3100, PIO1)
 *   [4] WS2812B LED strip (GPIO21, PIO0)
 *
 * UART debug output on GPIO0 (TX) / GPIO1 (RX) at 115200 baud.
 * Connect Debug Probe: TX(orange)->GPIO1, RX(yellow)->GPIO0, GND->GND.
 *
 * Main loop:
 *   - Every 500 ms: print joystick X/Y and raw button register
 *   - Every 2 s: advance LED animation frame
 *   - Every 4 s: cycle LED pattern
 *   - Button A: play I2S chime
 *   - Button B: play I2S scale
 *   - Button X: LED rainbow cycle (short)
 *   - Button Y: toggle display test pattern
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "config.h"
#include "adc_joystick.h"       // joystick_init / joystick_read / joystick_sw_consume
#include "gpio_buttons.h"       // buttons_init / buttons_set_callback
#include "led_strip.h"          // led_strip_init / led_strip_* (WS2812B)
#include "i2s_audio.h"          // i2s_audio_init / i2s_play_chime / i2s_play_scale
#include "spi_display.h"        // display_init / display_clear / display_fill_rect / display_set_pixel

// ── Forward declarations (display_* are in hstx_display.c) ─────────────────
void display_init(void);
void display_clear(uint8_t r, uint8_t g, uint8_t b);
void display_fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void display_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
bool display_is_ready(void);

// ── I2C scan ────────────────────────────────────────────────────────────────
static void i2c_scan(void)
{
    printf("[I2C] Scanning bus 1 (SDA=GPIO%d, SCL=GPIO%d)...\n",
           I2C_SDA_PIN, I2C_SCL_PIN);
    int found = 0;
    for (int addr = 0x08; addr < 0x78; addr++)
    {
        uint8_t buf;
        int ret = i2c_read_blocking(I2C_PORT, addr, &buf, 1, false);
        if (ret >= 0)
        {
            printf("[I2C]   0x%02X  ← device found\n", addr);
            found++;
        }
    }
    printf("[I2C] Scan complete — %d device(s) found.\n\n", found);
}

// ── Display test patterns ───────────────────────────────────────────────────
static int display_pattern = 0;

static void draw_display_pattern(void)
{
    switch (display_pattern % 4)
    {
    case 0: // Colour bars
        display_fill_rect(  0, 0, 160, 480, 255,   0,   0); // Red
        display_fill_rect(160, 0, 160, 480,   0, 255,   0); // Green
        display_fill_rect(320, 0, 160, 480,   0,   0, 255); // Blue
        display_fill_rect(480, 0, 160, 480, 255, 255,   0); // Yellow
        printf("[DISP] Pattern 0: colour bars\n");
        break;
    case 1: // White grid
        display_clear(0, 0, 0);
        for (int x = 0; x < 640; x += 64)
            display_fill_rect(x, 0, 2, 480, 255, 255, 255);
        for (int y = 0; y < 480; y += 60)
            display_fill_rect(0, y, 640, 2, 255, 255, 255);
        printf("[DISP] Pattern 1: white grid\n");
        break;
    case 2: // Cyan fill
        display_clear(0, 200, 255);
        printf("[DISP] Pattern 2: cyan fill\n");
        break;
    case 3: // Checkerboard (8×8 blocks)
        for (int by = 0; by < 480; by += 60)
            for (int bx = 0; bx < 640; bx += 64)
            {
                bool white = (((bx / 64) + (by / 60)) & 1) == 0;
                display_fill_rect(bx, by, 64, 60,
                                  white ? 255 : 30,
                                  white ? 255 : 30,
                                  white ? 255 : 30);
            }
        printf("[DISP] Pattern 3: checkerboard\n");
        break;
    }
}

// ── LED animations ──────────────────────────────────────────────────────────
static uint8_t led_hue       = 0;   // rainbow position
static uint8_t led_chase_pos = 0;   // chase position
static int     led_mode      = 0;   // current animation mode

static void led_tick(void)          // called every ~500 ms
{
    switch (led_mode % 3)
    {
    case 0: // Rotating rainbow
        for (int i = 0; i < WS2812_NUM_LEDS; i++)
        {
            uint8_t h = (uint8_t)(led_hue + (i * 255 / WS2812_NUM_LEDS));
            led_strip_set_pixel_hsv(i, h, 255, 200);
        }
        led_hue += 10;
        break;
    case 1: // Comet chase (single white pixel)
        led_strip_clear();
        led_strip_set_pixel(led_chase_pos % WS2812_NUM_LEDS, 255, 255, 255);
        // dim trailing pixels
        for (int t = 1; t <= 3; t++)
        {
            int idx = ((int)led_chase_pos - t + WS2812_NUM_LEDS) % WS2812_NUM_LEDS;
            uint8_t dim = (uint8_t)(128 >> t);
            led_strip_set_pixel(idx, dim, dim, dim);
        }
        led_chase_pos = (led_chase_pos + 1) % WS2812_NUM_LEDS;
        break;
    case 2: // Slow red pulse
    {
        static uint8_t pulse = 0;
        static int8_t  dir   = 8;
        led_strip_set_all(pulse, 0, 0);
        if (pulse + dir > 200 || (int)pulse + dir < 10) dir = -dir;
        pulse = (uint8_t)(pulse + dir);
        break;
    }
    }
    led_strip_show();
}

// ── Button callback ─────────────────────────────────────────────────────────
static volatile bool play_chime = false;
static volatile bool play_scale = false;
static volatile bool do_rainbow = false;
static volatile bool next_pattern = false;

static void button_event(Button btn, ButtonState state)
{
    if (state != BTN_STATE_PRESSED) return;
    const char *names[] = {"A", "B", "X", "Y"};
    printf("[BTN] %s PRESSED\n", names[btn]);
    switch (btn)
    {
    case BTN_A: play_chime  = true; break;
    case BTN_B: play_scale  = true; break;
    case BTN_X: do_rainbow  = true; break;
    case BTN_Y: next_pattern = true; break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
int main(void)
{
    // ── UART stdio — no USB enumeration needed ─────────────────────────
    stdio_uart_init_full(uart0, 115200, 0, 1); // TX=GPIO0, RX=GPIO1
    sleep_ms(200);

    printf("\n\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   PioNeer Full Peripheral Test       ║\n");
    printf("║   Adafruit Feather RP2350 HSTX       ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // ── [1] System clock ───────────────────────────────────────────────
    printf("[1/5] Setting sys clock to 150 MHz...\n");
    set_sys_clock_khz(150000, true);
    stdio_uart_init_full(uart0, 115200, 0, 1); // re-init after clk change
    printf("[1/5] Done. sys_clk = %lu Hz\n\n", clock_get_hz(clk_sys));

    // ── [2] HSTX DVI display ───────────────────────────────────────────
    printf("[2/5] Initializing HSTX DVI display (640x480@60Hz)...\n");
    display_init();
    printf("[2/5] HSTX display initialized.\n");
    draw_display_pattern();   // show colour bars immediately
    printf("\n");

    // ── [3] I2C bus + I2C scan ─────────────────────────────────────────
    printf("[3/5] Initializing I2C1 (SDA=GPIO%d, SCL=GPIO%d, 400kHz)...\n",
           I2C_SDA_PIN, I2C_SCL_PIN);
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    printf("[3/5] I2C1 initialized.\n");
    i2c_scan();

    // ── [4] Joystick + Buttons (Adafruit 5743 over I2C) ───────────────
    printf("[4/5] Initializing I2C joystick (Adafruit 5743 @ 0x%02X)...\n",
           JOYSTICK_I2C_ADDR);
    joystick_init();
    joystick_sw_init();
    buttons_init();
    buttons_set_callback(button_event);
    printf("[4/5] Joystick + buttons initialized.\n\n");

    // ── [5] I2S audio (TLV320DAC3100 on PIO1) ─────────────────────────
    printf("[5/5] Initializing I2S audio (TLV320DAC3100 @ 0x%02X)...\n",
           DAC_I2C_ADDR);
    bool audio_ok = i2s_audio_init();
    printf("[5/5] I2S audio %s.\n\n", audio_ok ? "initialized" : "FAILED (check wiring)");

    // ── [6] WS2812B LED strip (PIO0, GPIO21) ──────────────────────────
    printf("[6/6] Initializing WS2812B strip (%d LEDs on GPIO%d)...\n",
           WS2812_NUM_LEDS, WS2812_PIN);
    led_strip_init();
    led_strip_set_brightness(150);
    printf("[6/6] LED strip initialized.\n\n");

    // ── Play startup chime ─────────────────────────────────────────────
    if (audio_ok)
    {
        printf("[AUDIO] Playing startup chime...\n");
        i2s_play_chime();
    }

    printf("╔══════════════════════════════════════╗\n");
    printf("║  Boot complete! All peripherals up.  ║\n");
    printf("║                                      ║\n");
    printf("║  BTN A  → I2S chime                  ║\n");
    printf("║  BTN B  → I2S scale                  ║\n");
    printf("║  BTN X  → LED rainbow burst          ║\n");
    printf("║  BTN Y  → next display pattern       ║\n");
    printf("║                                      ║\n");
    printf("║  Joystick X/Y polled every 500 ms    ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // ══ Main loop ══════════════════════════════════════════════════════
    uint32_t last_joy_print  = 0;
    uint32_t last_led_tick   = 0;
    uint32_t last_led_mode   = 0;
    uint32_t loop_count      = 0;

    while (true)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // ── Joystick poll (500 ms) ─────────────────────────────────────
        if (now - last_joy_print >= 500)
        {
            last_joy_print = now;
            JoystickReading joy = joystick_read();
            bool sw = joystick_sw_consume();
            printf("[JOY] #%lu  X=%-5d Y=%-5d SW=%d\n",
                   loop_count++, (int)joy.x, (int)joy.y, (int)sw);
        }

        // ── LED animation tick (500 ms) ────────────────────────────────
        if (now - last_led_tick >= 500)
        {
            last_led_tick = now;
            led_tick();
        }

        // ── LED mode rotation (8 s) ────────────────────────────────────
        if (now - last_led_mode >= 8000)
        {
            last_led_mode = now;
            led_mode++;
            printf("[LED] Switching to animation mode %d\n", led_mode % 3);
        }

        // ── Button-triggered actions (handled in callback / flags) ─────
        if (play_chime)
        {
            play_chime = false;
            printf("[AUDIO] Playing chime...\n");
            i2s_play_chime();
        }
        if (play_scale)
        {
            play_scale = false;
            printf("[AUDIO] Playing scale...\n");
            i2s_play_scale();
        }
        if (do_rainbow)
        {
            do_rainbow = false;
            printf("[LED] Rainbow burst!\n");
            led_strip_fill_rainbow(0, 25);
            led_strip_show();
            sleep_ms(800);
        }
        if (next_pattern)
        {
            next_pattern = false;
            display_pattern++;
            draw_display_pattern();
        }

        sleep_ms(10);
    }

    return 0;
}