/**
 * HSTX DVI Display Driver for Adafruit Feather RP2350 HSTX
 *
 * Based on the official Raspberry Pi pico-examples:
 *   hstx/dvi_out_hstx_encoder/dvi_out_hstx_encoder.c
 *
 * Generates 640x480 @ ~60Hz DVI output using the RP2350's HSTX peripheral
 * with hardware TMDS encoding. Uses RGB332 (1 byte/pixel) colour format
 * so the full 640x480 framebuffer is ~300 KB, fitting in SRAM.
 *
 * Feather HSTX 22-pin FPC pinout (matches Pico DVI Sock):
 *   GP12 D2+   GP13 D2-
 *   GP14 CK+   GP15 CK-
 *   GP16 D1+   GP17 D1-
 *   GP18 D0+   GP19 D0-
 */

#include "spi_display.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/regs/hstx_ctrl.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

// ── 640×480 @ 60 Hz VGA/DVI timing (in pixels / lines) ─────────────────────
#define MODE_H_ACTIVE_PIXELS  640
#define MODE_H_FRONT_PORCH     16
#define MODE_H_SYNC_WIDTH      96
#define MODE_H_BACK_PORCH      48

#define MODE_V_ACTIVE_LINES   480
#define MODE_V_FRONT_PORCH     10
#define MODE_V_SYNC_WIDTH       2
#define MODE_V_BACK_PORCH      33

#define MODE_H_TOTAL_PIXELS (MODE_H_FRONT_PORCH + MODE_H_SYNC_WIDTH + \
                             MODE_H_BACK_PORCH  + MODE_H_ACTIVE_PIXELS)
#define MODE_V_TOTAL_LINES  (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + \
                             MODE_V_BACK_PORCH  + MODE_V_ACTIVE_LINES)

// ── HSTX command expander opcodes ───────────────────────────────────────────
#define HSTX_CMD_RAW        (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT (0x1u << 12)
#define HSTX_CMD_TMDS       (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP        (0xfu << 12)

// ── TMDS sync symbols (active-low hsync / vsync encoded as control tokens) ──
// DVI control period symbols for each combination of vsync, hsync.
// These are full 32-bit words containing three 10-bit TMDS control symbols
// packed for the HSTX raw output mode.

// TMDS control symbols (active-low):
//   D = 0b1101010100  (hsync=0, vsync=0)
//   D = 0b0010101011  (hsync=1, vsync=0)
//   D = 0b0101010100  (hsync=0, vsync=1)
//   D = 0b1010101011  (hsync=1, vsync=1)

#define TMDS_CTRL_00 0x354u  // vsync=0 hsync=0
#define TMDS_CTRL_01 0x0abu  // vsync=0 hsync=1
#define TMDS_CTRL_10 0x154u  // vsync=1 hsync=0
#define TMDS_CTRL_11 0x2abu  // vsync=1 hsync=1

// Pack three 10-bit TMDS symbols for lanes 0, 1, 2 into a 32-bit word.
// Lane 2 = blue, lane 1 = green (carries hsync/vsync), lane 0 = red
// For sync, all three lanes carry the same control symbol.
#define SYNC_V0_H0 ((TMDS_CTRL_00) | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 ((TMDS_CTRL_01) | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_01 << 20))
#define SYNC_V1_H0 ((TMDS_CTRL_10) | (TMDS_CTRL_10 << 10) | (TMDS_CTRL_10 << 20))
#define SYNC_V1_H1 ((TMDS_CTRL_11) | (TMDS_CTRL_11 << 10) | (TMDS_CTRL_11 << 20))

// ── DMA channels ────────────────────────────────────────────────────────────
#define DMACH_PING 0
#define DMACH_PONG 1

// ── Framebuffer (RGB332, 1 byte per pixel) ──────────────────────────────────
static uint8_t framebuf[MODE_H_ACTIVE_PIXELS * MODE_V_ACTIVE_LINES];

// ── HSTX command lists ──────────────────────────────────────────────────────
// Padded with NOPs to be >= HSTX FIFO size (8 words).

static uint32_t vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_NOP
};

static uint32_t vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V0_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_NOP
};

static uint32_t vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS       | MODE_H_ACTIVE_PIXELS
};

// ── DMA state ───────────────────────────────────────────────────────────────
static bool dma_pong = false;
static uint v_scanline = 2;
static bool vactive_cmdlist_posted = false;

// ── DMA IRQ handler (runs in SRAM for speed) ────────────────────────────────
void __scratch_x("") hstx_dma_irq_handler(void)
{
    uint ch_num = dma_pong ? DMACH_PONG : DMACH_PING;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    dma_pong = !dma_pong;

    if (v_scanline >= MODE_V_FRONT_PORCH &&
        v_scanline < (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH))
    {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    }
    else if (v_scanline < MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH)
    {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    }
    else if (!vactive_cmdlist_posted)
    {
        ch->read_addr = (uintptr_t)vactive_line;
        ch->transfer_count = count_of(vactive_line);
        vactive_cmdlist_posted = true;
    }
    else
    {
        // Send one scanline of pixel data
        uint line = v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES);
        ch->read_addr = (uintptr_t)&framebuf[line * MODE_H_ACTIVE_PIXELS];
        ch->transfer_count = MODE_H_ACTIVE_PIXELS / sizeof(uint32_t);
        vactive_cmdlist_posted = false;
    }

    if (!vactive_cmdlist_posted)
    {
        v_scanline = (v_scanline + 1) % MODE_V_TOTAL_LINES;
    }
}

// ── Public API ──────────────────────────────────────────────────────────────

void display_init(void)
{
    printf("[DISPLAY] Initializing HSTX DVI 640x480@60Hz...\n");

    // ── Configure TMDS encoder for RGB332 ──────────────────────────────
    // Lane 2 = blue  (3 bits at bit positions [7:5] → nbits=2, rot=0)
    // Lane 1 = green (3 bits at bit positions [4:2] → nbits=2, rot=29)
    // Lane 0 = red   (2 bits at bit positions [1:0] → nbits=1, rot=26)
    hstx_ctrl_hw->expand_tmds =
        2  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
        0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
        2  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
        29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
        1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
        26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

    // Pixels (TMDS): 4 shifts of 8 bits each per 32-bit word (4 pixels).
    // Control (RAW): 1 shift of the entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
        8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    // ── Serial output: clkdiv=5, n_shifts=5, shift=2 ──────────────────
    // 125 MHz HSTX clock / 5 = 25 MHz pixel clock → 250 Mbps TMDS bit rate
    // (DVI 480p60 needs 252 MHz; 250 is close enough for most monitors)
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // ── Pin assignment (matches Feather HSTX / Pico DVI Sock) ──────────
    // HSTX outputs 0-7 → GPIO 12-19
    //   GP12 D2+  GP13 D2-   (bit[0], bit[1])  ← but Adafruit wires D2 here
    //   GP14 CK+  GP15 CK-   (bit[2], bit[3])
    //   GP16 D1+  GP17 D1-   (bit[4], bit[5])  ← but Adafruit wires D1 here
    //   GP18 D0+  GP19 D0-   (bit[6], bit[7])

    // Clock pair on bit[2]/bit[3]:
    hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;

    // Data lanes: lane_to_output_bit maps TMDS lane → HSTX output bit index
    static const int lane_to_output_bit[3] = {0, 6, 4};
    for (uint lane = 0; lane < 3; ++lane)
    {
        int bit = lane_to_output_bit[lane];
        uint32_t lane_data_sel_bits =
            (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit    ] = lane_data_sel_bits;
        hstx_ctrl_hw->bit[bit + 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
    }

    // Set GPIO 12-19 to HSTX function (funcsel 0)
    for (int i = 12; i <= 19; ++i)
    {
        gpio_set_function(i, 0);
    }

    // ── Clear framebuffer ──────────────────────────────────────────────
    memset(framebuf, 0, sizeof(framebuf));

    // ── Set up ping-pong DMA ───────────────────────────────────────────
    dma_channel_config c;

    c = dma_channel_get_default_config(DMACH_PING);
    channel_config_set_chain_to(&c, DMACH_PONG);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        DMACH_PING, &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );

    c = dma_channel_get_default_config(DMACH_PONG);
    channel_config_set_chain_to(&c, DMACH_PING);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        DMACH_PONG, &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );

    // ── IRQ for scanline management ────────────────────────────────────
    dma_hw->ints0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
    dma_hw->inte0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
    irq_set_exclusive_handler(DMA_IRQ_0, hstx_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Give DMA bus priority so video never glitches
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS |
                            BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    // ── Start! ─────────────────────────────────────────────────────────
    dma_channel_start(DMACH_PING);

    printf("[DISPLAY] HSTX DVI output active on GPIO 12-19\n");
    printf("[DISPLAY] Framebuffer: %u bytes (RGB332)\n", (unsigned)sizeof(framebuf));
}

// ── Framebuffer access ──────────────────────────────────────────────────────

uint8_t *display_get_framebuf(void)
{
    return framebuf;
}

static inline uint8_t rgb332(uint8_t r, uint8_t g, uint8_t b)
{
    return (r & 0xc0) >> 6 | (g & 0xe0) >> 3 | (b & 0xe0);
}

void display_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= MODE_H_ACTIVE_PIXELS || y < 0 || y >= MODE_V_ACTIVE_LINES)
        return;
    framebuf[y * MODE_H_ACTIVE_PIXELS + x] = rgb332(r, g, b);
}

void display_fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t c = rgb332(r, g, b);
    for (int row = y; row < y + h && row < MODE_V_ACTIVE_LINES; row++)
    {
        if (row < 0) continue;
        for (int col = x; col < x + w && col < MODE_H_ACTIVE_PIXELS; col++)
        {
            if (col < 0) continue;
            framebuf[row * MODE_H_ACTIVE_PIXELS + col] = c;
        }
    }
}

void display_clear(uint8_t r, uint8_t g, uint8_t b)
{
    memset(framebuf, rgb332(r, g, b), sizeof(framebuf));
}

bool display_is_ready(void)
{
    return true;
}
