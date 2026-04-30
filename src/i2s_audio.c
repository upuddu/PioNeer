/**
 * I2S Audio Driver for Adafruit 6309 (TLV320DAC3100)
 *
 * Uses PIO1 for I2S output (PIO0 is used by WS2812B LEDs).
 * Uses I2C1 (shared with joystick) to configure the DAC registers.
 *
 * Wiring:
 *   BCK  → GPIO 9  (PIO1 side-set bit 1)
 *   WSEL → GPIO 10 (PIO1 side-set bit 0)
 *   DIN  → GPIO 11 (PIO1 OUT pin)
 *   SDA  → GPIO 2  (I2C1, shared with Adafruit 5743)
 *   SCL  → GPIO 3  (I2C1, shared with Adafruit 5743)
 */

#include "i2s_audio.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <math.h>

// ── PIO I2S program (pre-assembled) ────────────────────────────────────────
//
// .program i2s_out
// .side_set 2           ; bit0 = BCK (GPIO9), bit1 = WSEL (GPIO10)
//
// ; Left channel (WSEL=0, bit1=0): 16 bits
//     set x, 14          side 0b00      ; 0: BCK low, WSEL low
// left_loop:
//     out pins, 1        side 0b00      ; 1: shift data bit, BCK low
//     jmp x--, left_loop side 0b01      ; 2: BCK high (DAC samples), WSEL still 0
//     out pins, 1        side 0b00      ; 3: 16th bit, BCK low
//     nop                side 0b01      ; 4: BCK high for 16th bit
//
// ; Right channel (WSEL=1, bit1=1): 16 bits
//     set x, 14          side 0b10      ; 5: BCK low, WSEL high
// right_loop:
//     out pins, 1        side 0b10      ; 6: shift data bit, BCK low, WSEL high
//     jmp x--, right_loop side 0b11     ; 7: BCK high, WSEL high
//     out pins, 1        side 0b10      ; 8: 16th bit, BCK low
//     nop                side 0b11      ; 9: BCK high for 16th bit
//
// wrap_target = 0, wrap = 9

#define i2s_out_wrap_target 0
#define i2s_out_wrap        9

static const uint16_t i2s_out_program_instructions[] = {
    //                                     side   (bit1=WSEL, bit0=BCK)
    0xE02E, //  0: set x, 14              0b00  BCK=0, WSEL=0
    0x6001, //  1: out pins, 1            0b00  BCK=0, WSEL=0
    0x0841, //  2: jmp x--, 1             0b01  BCK=1, WSEL=0
    0x6001, //  3: out pins, 1            0b00  BCK=0, WSEL=0
    0xA842, //  4: nop (mov y, y)         0b01  BCK=1, WSEL=0
    0xF02E, //  5: set x, 14              0b10  BCK=0, WSEL=1
    0x7001, //  6: out pins, 1            0b10  BCK=0, WSEL=1
    0x1846, //  7: jmp x--, 6             0b11  BCK=1, WSEL=1
    0x7001, //  8: out pins, 1            0b10  BCK=0, WSEL=1
    0xB842, //  9: nop (mov y, y)         0b11  BCK=1, WSEL=1
};

static const struct pio_program i2s_out_program = {
    .instructions = i2s_out_program_instructions,
    .length = 10,
    .origin = -1,
    .pio_version = 0,
#if PICO_PIO_VERSION > 0
    .used_gpio_ranges = 0x0
#endif
};

// ── State ───────────────────────────────────────────────────────────────────
static PIO i2s_pio = pio1;  // Use PIO1 (PIO0 is for WS2812B)
static uint i2s_sm = 0;
static bool i2s_initialized = false;

// ── DAC I2C helpers ─────────────────────────────────────────────────────────

static bool dac_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    int ret = i2c_write_blocking(I2C_PORT, DAC_I2C_ADDR, buf, 2, false);
    if (ret == PICO_ERROR_GENERIC)
    {
        printf("[DAC] I2C write failed: reg=0x%02X val=0x%02X\n", reg, val);
        return false;
    }
    return true;
}

static bool dac_select_page(uint8_t page)
{
    return dac_write_reg(0x00, page);
}

// ── DAC initialization ─────────────────────────────────────────────────────
// Configure TLV320DAC3100 for:
//   - I2S slave mode, 16-bit
//   - PLL clocked from BCLK
//   - 44.1 kHz sample rate
//   - Headphone output enabled

static bool dac_init(void)
{
    printf("[DAC] Configuring TLV320DAC3100...\n");

    // ── Page 0 ─────────────────────────────────────────────────────────
    dac_select_page(0);

    // Software reset
    dac_write_reg(0x01, 0x01);
    sleep_ms(10);

    // Reg 4: Clock Mux = BCLK → PLL → CODEC_CLKIN
    //   bits[3:2] = 01 (PLL_CLKIN = BCLK)
    //   bits[1:0] = 11 (CODEC_CLKIN = PLL output)
    dac_write_reg(0x04, 0x07);

    // ── PLL config for 44.1 kHz from BCLK ──────────────────────────────
    // BCLK = 44100 * 32 = 1,411,200 Hz
    // We need DAC_CLK ~= 11,289,600 Hz (256 * fs)
    // PLL output = BCLK * R * J.D / P
    // With P=1, R=1, J=8, D=0: PLL = 1,411,200 * 8 = 11,289,600 Hz ✓
    //
    // Reg 5: PLL P & R  (bit7 = power up PLL, bits[6:4]=P, bits[3:0]=R)
    dac_write_reg(0x05, 0x91);  // PLL on, P=1, R=1
    // Reg 6: PLL J value
    dac_write_reg(0x06, 0x08);  // J=8
    // Reg 7-8: PLL D value (fractional, 14-bit)
    dac_write_reg(0x07, 0x00);  // D MSB = 0
    dac_write_reg(0x08, 0x00);  // D LSB = 0

    // ── Clock dividers ─────────────────────────────────────────────────
    // NDAC = 2, MDAC = 2, DOSR = 128
    // DAC_CLK = PLL / NDAC = 11,289,600 / 2 = 5,644,800
    // DAC_MOD_CLK = DAC_CLK / MDAC = 5,644,800 / 2 = 2,822,400
    // fs = DAC_MOD_CLK / DOSR = 2,822,400 / 64 = 44,100 ✓
    dac_write_reg(0x0B, 0x82);  // NDAC = 2, power on
    dac_write_reg(0x0C, 0x82);  // MDAC = 2, power on
    dac_write_reg(0x0D, 0x00);  // DOSR MSB = 0
    dac_write_reg(0x0E, 0x40);  // DOSR LSB = 64

    // Reg 27 (0x1B): Audio Interface
    //   I2S mode, 16-bit word, slave mode (BCLK/WCLK inputs)
    dac_write_reg(0x1B, 0x00);

    // Reg 60 (0x3C): DAC Processing Block = PRB_P1
    dac_write_reg(0x3C, 0x01);

    // Reg 63 (0x3F): DAC Data Path Setup
    //   Left  DAC → Left  data, Right DAC → Right data
    dac_write_reg(0x3F, 0xD4);  // both DACs on, left→left, right→right

    // Reg 65 (0x41): Left DAC volume = 0dB
    dac_write_reg(0x41, 0x00);
    // Reg 66 (0x42): Right DAC volume = 0dB
    dac_write_reg(0x42, 0x00);

    // Reg 64 (0x40): DAC Volume Control — unmute both channels
    dac_write_reg(0x40, 0x00);

    // ── Page 1: Analog routing & output drivers ────────────────────────
    dac_select_page(1);

    // Reg 31 (0x1F): Headphone drivers
    //   Power up HPL and HPR
    dac_write_reg(0x1F, 0xC4);

    // Reg 33 (0x21): HP output driver config
    dac_write_reg(0x21, 0x00);

    // Reg 35 (0x23): Route Left DAC → HPL
    dac_write_reg(0x23, 0x44);

    // Reg 36 (0x24): Route Right DAC → HPR
    dac_write_reg(0x24, 0x44);

    // Reg 40 (0x28): HPL analog volume = 0dB
    dac_write_reg(0x28, 0x06);

    // Reg 41 (0x29): HPR analog volume = 0dB
    dac_write_reg(0x29, 0x06);

    // Back to page 0
    dac_select_page(0);

    printf("[DAC] TLV320DAC3100 configured for 44.1kHz I2S slave mode\n");
    return true;
}

// ── PIO I2S init ────────────────────────────────────────────────────────────

static void pio_i2s_init(void)
{
    uint offset = pio_add_program(i2s_pio, &i2s_out_program);

    // Configure OUT pin (DIN = GPIO 11)
    pio_gpio_init(i2s_pio, I2S_DIN_PIN);
    pio_sm_set_consecutive_pindirs(i2s_pio, i2s_sm, I2S_DIN_PIN, 1, true);

    // Configure side-set pins (BCK = GPIO 9, WSEL = GPIO 10)
    // Side-set base = GPIO 9, so bit0 → GPIO 9 (BCK), bit1 → GPIO 10 (WSEL)
    // Wait — side-set bit ordering: bit0 = base pin, bit1 = base+1
    // We want: side-set bit 1 = BCK (GPIO 9), bit 0 = WSEL (GPIO 10)
    // But side_set_base assigns consecutive pins: base, base+1
    // If base = 9: bit0 → GPIO 9 (BCK), bit1 → GPIO 10 (WSEL)
    //
    // Our PIO program uses: side bit1=BCK, bit0=WSEL
    // So we need base = GPIO 10 for WSEL (bit0=10), BCK at bit1=GPIO 11?
    // No, that conflicts with DIN on GPIO 11.
    //
    // Let me reconsider the pin assignment:
    // Actually, with side_set_base = 9:
    //   side bit 0 → GPIO 9
    //   side bit 1 → GPIO 10
    // In our program, side bits are: bit1=BCLK, bit0=WSEL
    // So: BCLK = GPIO 10, WSEL = GPIO 9
    //
    // But user said BCK=GPIO9, WSEL=GPIO10. So we need to swap in the program
    // OR set the base differently.
    //
    // Easiest: set side_set base = 9, then:
    //   bit0 = GPIO 9 = BCK
    //   bit1 = GPIO 10 = WSEL
    // And in our PIO program, reinterpret:
    //   side 0b00 = BCK low, WSEL low
    //   side 0b01 = BCK high, WSEL low    ← left channel
    //   side 0b10 = BCK low, WSEL high    ← right channel transition
    //   side 0b11 = BCK high, WSEL high
    //
    // Our program already encodes this as:
    //   Left: BCK toggles between 0b00/0b10 (bit1 toggles) while bit0=0
    //   Right: BCK toggles between 0b01/0b11 while bit0=1
    //
    // If bit0=BCK(GPIO9), bit1=WSEL(GPIO10):
    //   Left: want BCK toggle, WSEL=0 → toggle bit0, keep bit1=0 → 0b00/0b01
    //   Right: want BCK toggle, WSEL=1 → toggle bit0, keep bit1=1 → 0b10/0b11
    //
    // Our program has left=0b00/0b10 and right=0b01/0b11, which means:
    //   bit1 = BCK, bit0 = WSEL
    //   bit1 → GPIO 10 (BCK?), bit0 → GPIO 9 (WSEL?)
    //
    // This swaps BCK and WSEL vs the user's wiring! Let's fix by using
    // side_set base = 9 and swapping our program's side-set meaning:
    //   Actually it's simpler to just reassign: base = 9 means
    //   GPIO 9 = bit0 (WSEL in our program), GPIO 10 = bit1 (BCK in our program)
    //   So with our PIO code as-is: BCK = GPIO 10, WSEL = GPIO 9
    //   But user wants BCK = GPIO 9, WSEL = GPIO 10.
    //
    // Fix: swap the pins in our mental model. Set side_set base to 9.
    // The PIO program side bits map to:
    //   Bit 0 → GPIO 9
    //   Bit 1 → GPIO 10
    // In our program, bit 1 toggles for BCK, bit 0 changes for WSEL.
    // So BCK → GPIO 10, WSEL → GPIO 9.
    //
    // This doesn't match the user's wiring (BCK=9, WSEL=10).
    // Fix: swap the side-set assignment. If we set base = 10:
    //   bit 0 → GPIO 10, bit 1 → GPIO 11
    //   That conflicts with DIN on GPIO 11.
    //
    // Better fix: just swap the bit meaning in our PIO program.
    // In the program: bit0 = BCK, bit1 = WSEL. With base=9:
    //   BCK = GPIO 9 ✓, WSEL = GPIO 10 ✓
    // We need to adjust the side-set values in the program.

    pio_gpio_init(i2s_pio, I2S_BCK_PIN);
    pio_gpio_init(i2s_pio, I2S_WSEL_PIN);
    pio_sm_set_consecutive_pindirs(i2s_pio, i2s_sm, I2S_BCK_PIN, 2, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + i2s_out_wrap_target, offset + i2s_out_wrap);

    // Side-set: 2 pins starting at GPIO 9 (BCK=bit0=GPIO9, WSEL=bit1=GPIO10)
    sm_config_set_sideset_pins(&c, I2S_BCK_PIN);
    sm_config_set_sideset(&c, 2, false, false);

    // OUT pin: DIN = GPIO 11
    sm_config_set_out_pins(&c, I2S_DIN_PIN, 1);

    // Shift: MSB first (left shift), autopull at 32 bits
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Clock divider: each bit takes 2 PIO cycles (data + clock edge)
    // BCLK = PIO_CLK / (2 * clkdiv)
    // We want BCLK = sample_rate * 32 = 44100 * 32 = 1,411,200 Hz
    // PIO_CLK = 150 MHz (RP2350 default)
    // clkdiv = PIO_CLK / (BCLK * 2) = 150,000,000 / (1,411,200 * 2) ≈ 53.14
    float sys_clk = (float)clock_get_hz(clk_sys);
    float target_bclk = (float)I2S_SAMPLE_RATE * 32.0f;
    float div = sys_clk / (target_bclk * 2.0f);
    sm_config_set_clkdiv(&c, div);

    printf("[I2S] System clock: %.0f Hz\n", sys_clk);
    printf("[I2S] Target BCLK: %.0f Hz, PIO clkdiv: %.2f\n", target_bclk, div);

    pio_sm_init(i2s_pio, i2s_sm, offset, &c);
    pio_sm_set_enabled(i2s_pio, i2s_sm, true);

    printf("[I2S] PIO1 SM%d started: BCK=GPIO%d, WSEL=GPIO%d, DIN=GPIO%d\n",
           i2s_sm, I2S_BCK_PIN, I2S_WSEL_PIN, I2S_DIN_PIN);
}

// ── Public API ──────────────────────────────────────────────────────────────

bool i2s_audio_init(void)
{
    printf("[I2S] Initializing I2S audio (Adafruit 6309 / TLV320DAC3100)...\n");

    // I2C is already initialized by joystick_init() on I2C1
    // Just configure the DAC registers
    if (!dac_init())
    {
        printf("[I2S] WARNING: DAC I2C init failed — check wiring!\n");
        // Continue anyway; PIO will still output I2S waveforms
    }

    pio_i2s_init();

    // Send a few frames of silence to prime the FIFO
    for (int i = 0; i < 8; i++)
    {
        pio_sm_put_blocking(i2s_pio, i2s_sm, 0);
    }

    i2s_initialized = true;
    printf("[I2S] Audio initialized.\n");
    return true;
}

void i2s_play_tone(float freq_hz, uint32_t duration_ms)
{
    if (!i2s_initialized) return;

    uint32_t total_samples = (I2S_SAMPLE_RATE * duration_ms) / 1000;
    float phase_inc = (2.0f * M_PI * freq_hz) / (float)I2S_SAMPLE_RATE;
    float phase = 0.0f;

    for (uint32_t i = 0; i < total_samples; i++)
    {
        // Generate 16-bit signed sine wave
        int16_t sample = (int16_t)(sinf(phase) * 16000.0f);
        phase += phase_inc;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

        // Pack stereo: [left 16-bit | right 16-bit] into 32-bit word
        uint16_t usample = (uint16_t)sample;
        uint32_t stereo = ((uint32_t)usample << 16) | (uint32_t)usample;
        pio_sm_put_blocking(i2s_pio, i2s_sm, stereo);
    }
}

void i2s_silence(void)
{
    if (!i2s_initialized) return;
    // Push a few frames of silence
    for (int i = 0; i < 64; i++)
    {
        pio_sm_put_blocking(i2s_pio, i2s_sm, 0);
    }
}

void i2s_play_chime(void)
{
    i2s_play_tone(523.0f, 100);  // C5
    i2s_play_tone(659.0f, 100);  // E5
    i2s_play_tone(784.0f, 200);  // G5
    i2s_silence();
}

void i2s_play_scale(void)
{
    float notes[] = {262, 294, 330, 349, 392, 440, 494, 523};
    for (int i = 0; i < 8; i++)
    {
        i2s_play_tone(notes[i], 200);
    }
    i2s_silence();
}
