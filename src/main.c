/**
 * PioNeer Peripheral Bringup — Step-by-Step, No HSTX
 *
 * Proof-of-life approach: WS2812B blinks FIRST (no serial needed),
 * then serial opens, then I2C, then I2S.
 *
 * Serial:   USB CDC (open the port in your terminal)
 * UART alt: GPIO0 TX / GPIO1 RX, 115200 baud (debug probe)
 *
 * LED colour key (GPIO21 WS2812B):
 *   BLUE pulsing  → alive, before serial
 *   GREEN         → all peripherals OK
 *   RED           → a peripheral failed
 *   YELLOW        → I2C no devices found
 *   CYAN (chase)  → normal running loop
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

#include "config.h"
#include "adc_joystick.h"   // joystick_init / joystick_read / joystick_sw_consume
#include "gpio_buttons.h"   // buttons_init / buttons_set_callback / button_get_state
#include "i2s_audio.h"      // i2s_audio_init / i2s_play_chime / i2s_play_scale
#include "led_strip.h"      // led_strip_init / led_strip_* (WS2812B on PIO0)

// ── tiny WS2812B heartbeat BEFORE full led_strip_init ──────────────────────
// Uses the same PIO program that led_strip_init will use later; we just
// force a single pixel colour directly so we know the MCU is running.
static void ws2812_set_color_raw(uint8_t r, uint8_t g, uint8_t b)
{
    // GRB order, shifted to top 24 bits (MSB first via shift_right=true in led_strip_init)
    uint32_t grb = ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
    // Push through whichever PIO/SM led_strip uses (pio0, sm0)
    if (pio_sm_is_tx_fifo_full(pio0, 0)) return;
    pio_sm_put_blocking(pio0, 0, grb);
    sleep_us(300); // reset pulse
}

// ── I2C bus scan ────────────────────────────────────────────────────────────
static int i2c_scan(void)
{
    printf("[I2C] Scanning 0x08..0x77 on I2C%d (SDA=GPIO%d, SCL=GPIO%d)...\n",
           (I2C_PORT == i2c0) ? 0 : 1, I2C_SDA_PIN, I2C_SCL_PIN);
    int found = 0;
    for (int addr = 0x08; addr < 0x78; addr++)
    {
        uint8_t buf;
        if (i2c_read_blocking(I2C_PORT, addr, &buf, 1, false) >= 0)
        {
            printf("[I2C]   0x%02X  ← device\n", addr);
            found++;
        }
    }
    printf("[I2C] Done — %d device(s).\n\n", found);
    return found;
}

// ── Button callback ─────────────────────────────────────────────────────────
static volatile bool play_chime   = false;
static volatile bool play_scale   = false;
static volatile bool do_rainbow   = false;

static void button_event(Button btn, ButtonState state)
{
    if (state != BTN_STATE_PRESSED) return;
    const char *n[] = {"A", "B", "X", "Y"};
    printf("[BTN] %s pressed\n", n[btn]);
    switch (btn)
    {
    case BTN_A: play_chime = true;  break;
    case BTN_B: play_scale = true;  break;
    case BTN_X: do_rainbow = true;  break;
    default: break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
int main(void)
{
    // ── STEP 0: LED strip init — proof of life BEFORE anything else ────
    // We init the LED PIO first so we have a visual heartbeat even if
    // serial / I2C / I2S all fail.
    led_strip_init();
    led_strip_set_brightness(100);

    // Flash BLUE three times = "MCU is alive"
    for (int i = 0; i < 3; i++)
    {
        led_strip_set_all(0, 0, 80);
        led_strip_show();
        sleep_ms(200);
        led_strip_clear();
        led_strip_show();
        sleep_ms(200);
    }
    // Leave BLUE solid while booting
    led_strip_set_all(0, 0, 60);
    led_strip_show();

    // ── STEP 1: UART + USB CDC stdio ──────────────────────────────────
    // Both UART (GPIO0/1) and USB CDC are enabled so you can use either.
    stdio_uart_init_full(uart0, 115200, 0, 1);
    stdio_usb_init();
    sleep_ms(300);  // let USB enumerate

    printf("\n\n");
    printf("=========================================\n");
    printf("  PioNeer Peripheral Bringup\n");
    printf("  Adafruit Feather RP2350 HSTX\n");
    printf("  Build: " __DATE__ " " __TIME__ "\n");
    printf("=========================================\n\n");

    // ── STEP 2: System clock (125 MHz — safe default) ─────────────────
    printf("[1] Setting sys clock to 125 MHz...\n");
    set_sys_clock_khz(125000, true);
    stdio_uart_init_full(uart0, 115200, 0, 1); // re-init UART after clk change
    printf("[1] sys_clk = %lu Hz\n\n", clock_get_hz(clk_sys));

    // ── STEP 3: I2C bus ────────────────────────────────────────────────
    printf("[2] Initializing I2C%d (SDA=GPIO%d SCL=GPIO%d 400kHz)...\n",
           (I2C_PORT == i2c0) ? 0 : 1, I2C_SDA_PIN, I2C_SCL_PIN);
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    printf("[2] I2C initialized.\n\n");

    int n_i2c = i2c_scan();
    if (n_i2c == 0)
    {
        // Yellow = I2C bus empty (not fatal — maybe nothing connected)
        led_strip_set_all(80, 60, 0);
        led_strip_show();
        sleep_ms(500);
    }

    // ── STEP 4: Joystick + Buttons (Adafruit 5743) ────────────────────
    printf("[3] Initializing joystick (Adafruit 5743 @ 0x%02X)...\n",
           JOYSTICK_I2C_ADDR);
    joystick_init();
    joystick_sw_init();
    buttons_init();
    buttons_set_callback(button_event);
    printf("[3] Joystick + buttons ready.\n\n");

    // ── STEP 5: I2S audio (TLV320DAC3100, PIO1) ───────────────────────
    printf("[4] Initializing I2S audio (DAC @ 0x%02X, PIO1)...\n",
           DAC_I2C_ADDR);
    bool audio_ok = i2s_audio_init();
    if (audio_ok)
    {
        printf("[4] I2S OK — playing startup chime...\n");
        i2s_play_chime();
    }
    else
    {
        printf("[4] I2S FAILED — continuing without audio.\n");
    }
    printf("\n");

    // ── Boot complete indicator ────────────────────────────────────────
    if (audio_ok && n_i2c > 0)
    {
        led_strip_set_all(0, 100, 0);   // GREEN = all good
        led_strip_show();
    }
    else if (n_i2c == 0)
    {
        led_strip_set_all(80, 60, 0);   // YELLOW = I2C empty
        led_strip_show();
    }
    else
    {
        led_strip_set_all(80, 0, 0);    // RED = audio failed
        led_strip_show();
    }
    sleep_ms(800);

    printf("=========================================\n");
    printf("  Boot complete!\n");
    printf("\n");
    printf("  BTN A  -> I2S chime\n");
    printf("  BTN B  -> I2S scale\n");
    printf("  BTN X  -> LED rainbow\n");
    printf("\n");
    printf("  Joystick X/Y printed every 500 ms\n");
    printf("=========================================\n\n");

    // ══ Main loop ══════════════════════════════════════════════════════
    uint32_t last_joy   = 0;
    uint32_t last_led   = 0;
    uint32_t loop_n     = 0;
    uint8_t  chase_pos  = 0;
    uint8_t  hue        = 0;

    while (true)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Joystick + button state every 500 ms
        if (now - last_joy >= 500)
        {
            last_joy = now;
            JoystickReading joy = joystick_read();
            bool sw = joystick_sw_consume();

            // Map 0-65535 to signed centre
            int16_t jx = (int16_t)(joy.x - 32768);
            int16_t jy = (int16_t)(joy.y - 32768);

            ButtonState ba = button_get_state(BTN_A);
            ButtonState bb = button_get_state(BTN_B);
            ButtonState bx = button_get_state(BTN_X);
            ButtonState by = button_get_state(BTN_Y);

            printf("[JOY] #%-4lu  X=%+6d  Y=%+6d  SW=%d  |  A=%d B=%d X=%d Y=%d\n",
                   loop_n++, jx, jy, (int)sw,
                   ba == BTN_STATE_PRESSED ? 1 : 0,
                   bb == BTN_STATE_PRESSED ? 1 : 0,
                   bx == BTN_STATE_PRESSED ? 1 : 0,
                   by == BTN_STATE_PRESSED ? 1 : 0);
        }

        // LED cyan comet chase — 80 ms per step
        if (now - last_led >= 80)
        {
            last_led = now;

            // Fade existing pixels
            for (int i = 0; i < WS2812_NUM_LEDS; i++)
            {
                uint8_t r, g, b;
                led_strip_get_pixel_rgb(i, &r, &g, &b);
                r = r > 15 ? r - 15 : 0;
                g = g > 15 ? g - 15 : 0;
                b = b > 15 ? b - 15 : 0;
                led_strip_set_pixel(i, r, g, b);
            }
            // Head pixel — rainbow colour
            uint8_t hr, hg, hb;
            // simple HSV→RGB (s=255, v=180)
            led_strip_set_pixel_hsv(chase_pos, hue, 255, 180);
            chase_pos = (chase_pos + 1) % WS2812_NUM_LEDS;
            hue += 3;
            led_strip_show();
        }

        // Deferred button actions
        if (play_chime)  { play_chime = false;  i2s_play_chime(); }
        if (play_scale)  { play_scale = false;  i2s_play_scale(); }
        if (do_rainbow)
        {
            do_rainbow = false;
            printf("[LED] Rainbow burst!\n");
            led_strip_fill_rainbow(0, 25);
            led_strip_show();
            sleep_ms(1000);
        }

        sleep_ms(5);
    }

    return 0;
}