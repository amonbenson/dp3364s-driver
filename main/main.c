/*
 * main.c - DP3364S LED panel driver for the ESP32-S3
 *
 * Port of the Raspberry Pi display_dma.c. Same protocol, but the clock comes
 * from the LCD_CAM peripheral instead of being shaped by software, which is
 * what the Pi could never do cleanly.
 *
 * How it works:
 *   LCD_CAM runs in i80 (Intel 8080) mode as a plain 16-bit parallel output.
 *   Its PCLK output is the panel's CLK: hardware generated, exact 50% duty,
 *   no jitter, and free of any CPU involvement. The other 13 signals are
 *   data bus bits, so one 16-bit word is exactly one clock cycle:
 *
 *     bit  0..5   R1 G1 B1 R2 G2 B2
 *     bit  6..10  A B C D E
 *     bit 11      LAT
 *     bit 12      OE
 *
 *   GDMA feeds the peripheral from a descriptor chain that loops on itself,
 *   so the panel is refreshed continuously without any interrupts.
 *
 * The stream has two phases, chained together in hardware:
 *   phase 1 runs once and writes every chip register,
 *   phase 2 is one frame of display data and loops forever.
 *
 * The display samples on the rising edge of CLK - both the data and the latch
 * count that selects a command, which is what the n in send_latches(n) means.
 * The only exception is the SDR command, which samples both edges; that still
 * runs in software before the peripheral takes over the pins.
 */

#include <math.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_private/gdma.h"
#include "esp_private/periph_ctrl.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/dma_types.h"
#include "soc/gpio_sig_map.h"
#include "soc/lcd_cam_struct.h"

static const char *TAG = "dp3364";



/* ---- wiring ------------------------------------------------------------ */

/* Must match the actual panel wiring. GPIO 1..14 are all safe on the S3 -
 * they avoid the flash/PSRAM pins (26..37), USB (19, 20) and the strapping
 * pins (0, 45, 46). */
enum {
    PIN_R1 = 1,
    PIN_G1 = 2,
    PIN_B1 = 3,
    PIN_R2 = 4,
    PIN_G2 = 5,
    PIN_B2 = 6,
    PIN_A = 7,
    PIN_B = 8,
    PIN_C = 9,
    PIN_D = 10,
    PIN_E = 11,
    PIN_LAT = 12,
    PIN_OE = 13,
    PIN_CLK = 14, // driven by LCD_CAM PCLK, not by software
};

/* Bit position of each signal inside the 16-bit bus word */
enum {
    SBIT_R1 = 0,
    SBIT_G1,
    SBIT_B1,
    SBIT_R2,
    SBIT_G2,
    SBIT_B2,
    SBIT_A,
    SBIT_B,
    SBIT_C,
    SBIT_D,
    SBIT_E,
    SBIT_LAT,
    SBIT_OE,
};

/* Bus bit -> GPIO. Unused bits are -1 and stay unrouted. */
static const int8_t BUS_PINS[16] = {
    PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2,
    PIN_A, PIN_B, PIN_C, PIN_D, PIN_E,
    PIN_LAT, PIN_OE,
    -1, -1, -1,
};

static const uint8_t DATA_BITS[6] = { SBIT_R1, SBIT_G1, SBIT_B1, SBIT_R2, SBIT_G2, SBIT_B2 };

/* build_display_frame() transposes the six data lines with constant shifts,
 * which only works while they are bus bits 0..5 in this exact order. */
_Static_assert(SBIT_R1 == 0 && SBIT_G1 == 1 && SBIT_B1 == 2 && SBIT_R2 == 3 && SBIT_G2 == 4 && SBIT_B2 == 5,
               "the six data lines must stay on bus bits 0..5, in RGB1/RGB2 order");
static const uint8_t ADDR_BITS[5] = { SBIT_A, SBIT_B, SBIT_C, SBIT_D, SBIT_E }; // pin order for B, C swapped (might be a hardware issue)

/* PCLK = 160 MHz / LCD_CLK_DIV. 16 -> 10 MHz, 12 -> 13.3 MHz, 10 -> 16 MHz. */
#define LCD_CLK_DIV 16
#define DCLK_HZ (160000000 / LCD_CLK_DIV)



/* ---- panel geometry ---------------------------------------------------- */

#define NUM_CHANNELS 16 // individual outputs per chip
#define NUM_ROWS 32 // number of addressable rows (half the display height)
#define CHIPS_PER_CHAIN 8 // number of chips = display width / NUM_CHANNELS = 128 / 16
#define TOTAL_COLS (CHIPS_PER_CHAIN * NUM_CHANNELS) // display width in pixels

typedef struct {
    uint8_t addr, r, g, b;
} RegEntry;

static const RegEntry REGS[] = {
    { 0x02, 0x1f, 0x1f, 0x1f },
    { 0x03, 0x00, 0x00, 0x00 }, // Number of PWM display groups (0 = all groups/auto, 1 = disabled, 2..1f = 2..32 groups). More groups introduce flicker at low clock speeds, but give better color depth.
    { 0x04, 0x1a, 0x1a, 0x1a }, // Brightness resolution (0x00..0x1a) (PWM depth)
    { 0x05, 0x04, 0x04, 0x00 },
    { 0x06, 0x39, 0x39, 0x39 }, // GCLK divider: FGCLK = FDCLK * (reg0x06[2:0] + 1)
    { 0x07, 0x00, 0x0c, 0x0c },
    { 0x08, 0x7f, 0x7f, 0x7f }, // Current gain ("analog brightness"). Values != 7f cause color shift though.
    { 0x09, 0x68, 0x6b, 0x61 },
    { 0x0A, 0xbe, 0xbf, 0xbe },
    { 0x0B, 0x28, 0x2b, 0x31 },
    { 0x0C, 0x58, 0x58, 0x58 }, // Use PWM display mode 1: "High-gray data independent refresh synchronous mode"
    { 0x0D, 0x08, 0x12, 0x18 },
    { 0x0E, 0x08, 0x0b, 0x01 },
    { 0x0F, 0x20, 0x20, 0x20 },
    { 0x10, 0x00, 0x00, 0x00 },
};
#define NUM_REGS (int)(sizeof(REGS) / sizeof(REGS[0]))

#define REG_CLOCKS (3 + 1 + 14 + TOTAL_COLS + 1) // one register write
#define INIT_CLOCKS (NUM_REGS * REG_CLOCKS)
#define DATA_CLOCKS (NUM_ROWS * NUM_CHANNELS * CHIPS_PER_CHAIN * 16)
#define FRAME_CLOCKS (3 + DATA_CLOCKS + 1)

// how long the DMA takes to play one frame, rounded up
#define FRAME_PERIOD_MS ((FRAME_CLOCKS * 1000 + DCLK_HZ - 1) / DCLK_HZ)



/* ---- stream builder ---------------------------------------------------- */
/*
 * Each clock is one 16-bit word. The peripheral holds those levels for the
 * whole PCLK period and generates the rising edge inside it, so data is
 * always stable across the edge the chip samples on.
 */

static uint16_t *stream; // buffer currently being filled
static size_t stream_len; // clocks written so far
static uint16_t pending; // signal levels for the next clock

static inline void emit_clock(void) {
    stream[stream_len++] = pending;
}

static inline void pin_hi(int bit) {
    pending |= (uint16_t)(1u << bit);
}

static inline void pin_lo(int bit) {
    pending &= (uint16_t)~(1u << bit);
}

// Set the RGB1/2 data lines at once
static inline void set_rgb_bit(const uint16_t chain_words[6], int bit_index) {
    int shift = 15 - bit_index;

    for (int c = 0; c < 6; c++) {
        if ((chain_words[c] >> shift) & 1) {
            pin_hi(DATA_BITS[c]);
        } else {
            pin_lo(DATA_BITS[c]);
        }
    }
}

// Send n clocks with no RGB data
static void send_clocks(int n) {
    static const uint16_t zero[6] = { 0, 0, 0, 0, 0, 0 };

    set_rgb_bit(zero, 0);

    for (int i = 0; i < n; i++) {
        emit_clock();
    }
}

// Send n clocks with no RGB data and LAT high
static void send_latches(int n) {
    static const uint16_t zero[6] = { 0, 0, 0, 0, 0, 0 };
    set_rgb_bit(zero, 0);
    pin_hi(SBIT_LAT);

    for (int i = 0; i < n; i++) {
        emit_clock();
    }

    pin_lo(SBIT_LAT);
}

// Send a VSYNC pulse (3 latch clocks)
static inline void send_vsync(void) {
    send_latches(3);
}

// Copy the same 6 words (one for each chain) to all 8 chips in the chain, with a single LAT pulse at the end
static void send_to_allRGB(const uint16_t words[6], int latches) {
    for (int clock = 0; clock < TOTAL_COLS; clock++) {
        if (clock == TOTAL_COLS - latches) {
            pin_hi(SBIT_LAT);
        }

        set_rgb_bit(words, clock & 0x0f);
        emit_clock();
    }

    pin_lo(SBIT_LAT);
}

/* Set the 5-bit row address lines at once */
static void set_row_address(int row) {
    for (int i = 0; i < 5; i++) {
        if ((row >> i) & 1) {
            pin_hi(ADDR_BITS[i]);
        } else {
            pin_lo(ADDR_BITS[i]);
        }
    }
}

/* For the test pattern: convert hue to RGB13 */
static void hue_to_rgb13(float hue_deg, uint16_t *r, uint16_t *g, uint16_t *b) {
    float h = fmodf(hue_deg, 360.0f) / 60.0f;
    int i = (int)h;
    float f = h - (float)i;
    float q = 1.0f - f;
    float rr, gg, bb;

    switch (i) {
        case 0:
            rr = 1;
            gg = f;
            bb = 0;
            break;
        case 1:
            rr = q;
            gg = 1;
            bb = 0;
            break;
        case 2:
            rr = 0;
            gg = 1;
            bb = f;
            break;
        case 3:
            rr = 0;
            gg = q;
            bb = 1;
            break;
        case 4:
            rr = f;
            gg = 0;
            bb = 1;
            break;
        default:
            rr = 1;
            gg = 0;
            bb = q;
            break;
    }

    *r = (uint16_t)(rr * 8191.0f);
    *g = (uint16_t)(gg * 8191.0f);
    *b = (uint16_t)(bb * 8191.0f);
}

/* The rainbow repeats every 64 diagonal steps, so the whole pattern is one
 * table built once at startup. Keeping the frame build free of floating point
 * is most of what makes it fast enough to animate. */
#define HUE_STEPS 64
static uint16_t hue_lut[HUE_STEPS][3];
static int hue_phase; // advanced by the animation loop, in table steps

static void build_hue_lut(void) {
    for (int i = 0; i < HUE_STEPS; i++) {
        hue_to_rgb13(i * (360.0f / HUE_STEPS), &hue_lut[i][0], &hue_lut[i][1], &hue_lut[i][2]);
    }
}

/* ---- framebuffer ------------------------------------------------------- */
/*
 * A plain (x, y) RGB framebuffer, 13 bits per channel. Drawing writes here and
 * nothing else; build_display_frame() below turns it into bus words. The two
 * halves are fully independent - drawing code never has to know about scan
 * order, chip splitting, bit serialisation or the RGB1/RGB2 split, and it can
 * take as long as it likes because only the encode step has to be fast.
 */
#define PANEL_W TOTAL_COLS
#define PANEL_H (NUM_ROWS * 2)

static uint16_t framebuffer[PANEL_H][PANEL_W][3];

static inline void fb_set(int x, int y, uint16_t r, uint16_t g, uint16_t b) {
    framebuffer[y][x][0] = r;
    framebuffer[y][x][1] = g;
    framebuffer[y][x][2] = b;
}

/* Draw one frame. This is the only place the picture is decided - replace the
 * body with anything: sprites, text, video, per-pixel maths, whatever. The
 * scrolling rainbow is just an example that needs one variable of state. */
static void draw_frame(void) {
    for (int y = 0; y < PANEL_H; y++) {
        for (int x = 0; x < PANEL_W; x++) {
            const uint16_t *c = hue_lut[(x + y + hue_phase) & (HUE_STEPS - 1)];

            fb_set(x, y, c[0], c[1], c[2]);
        }
    }
}

/* Build the 6 chain words for one register entry */
static void reg_words(int reg, uint16_t words[6]) {
    const RegEntry *e = &REGS[reg];
    uint16_t w = (uint16_t)(e->addr << 8);

    words[0] = (uint16_t)(w | e->r);
    words[1] = (uint16_t)(w | e->g);
    words[2] = (uint16_t)(w | e->b);
    words[3] = (uint16_t)(w | e->r);
    words[4] = (uint16_t)(w | e->g);
    words[5] = (uint16_t)(w | e->b);
}

/* Phase 1: write every register once, back to back. Played a single time at
 * startup and then never again, so the display phase is never interrupted by
 * a register write. */
static void build_register_init(uint16_t *dst) {
    stream = dst;
    stream_len = 0;
    pending = 0;

    for (int reg = 0; reg < NUM_REGS; reg++) {
        uint16_t words[6];

        send_vsync();
        send_clocks(1);
        send_latches(14);

        reg_words(reg, words);
        send_to_allRGB(words, 5);

        // separates this register's latch from the next vsync
        send_clocks(1);
    }
}

/* Phase 2: encode the framebuffer into one frame of display data, preceded by
 * its own vsync. This is pure protocol - it does not care what was drawn.
 * Signal state and render_line carry over between calls; render_line advances
 * 512 times per frame and 512 % NUM_ROWS is 0, so it returns to its starting
 * value and the frame loops seamlessly. */
static void build_display_frame(uint16_t *dst) {
    static int render_line = 0;

    stream = dst;
    stream_len = 0;

    send_vsync();

    for (int line = 0; line < NUM_ROWS; line++) {
        for (int channel = 0; channel < NUM_CHANNELS; channel++) {
            pin_lo(SBIT_OE);

            for (int chip = 0; chip < CHIPS_PER_CHAIN; chip++) {
                int col = chip * NUM_CHANNELS + channel;
                const uint16_t *top = framebuffer[line][col];
                const uint16_t *bot = framebuffer[line + NUM_ROWS][col];

                /* Hot path: 65536 iterations per frame, and pure protocol -
                 * it serialises whatever the framebuffer holds, MSB first, the
                 * way the panel's shift registers want it. The six data lines
                 * are bus bits 0..5, so the transpose needs constant shifts
                 * only: no variable shift, no branch per line, and the signal
                 * state stays in a register rather than being read back from
                 * memory six times per clock. */
                uint16_t w0 = top[0], w1 = top[1], w2 = top[2];
                uint16_t w3 = bot[0], w4 = bot[1], w5 = bot[2];
                uint16_t p = pending & (uint16_t)~0x3fu;
                uint16_t *out = stream + stream_len;
                int lat_bit = (chip == CHIPS_PER_CHAIN - 1) ? 15 : -1;
                int oe_bit = (chip == 0) ? (render_line == 0 ? 12 : 4) : -1;

                for (int bit = 0; bit < 16; bit++) {
                    if (bit == lat_bit) {
                        p |= (uint16_t)(1u << SBIT_LAT);
                    }
                    if (bit == oe_bit) {
                        p |= (uint16_t)(1u << SBIT_OE);
                    }

                    *out++ = (uint16_t)(p | (w0 >> 15) | ((w1 >> 15) << 1) | ((w2 >> 15) << 2)
                                          | ((w3 >> 15) << 3) | ((w4 >> 15) << 4) | ((w5 >> 15) << 5));

                    w0 <<= 1;
                    w1 <<= 1;
                    w2 <<= 1;
                    w3 <<= 1;
                    w4 <<= 1;
                    w5 <<= 1;
                }

                stream_len = (size_t)(out - stream);
                pending = p & (uint16_t)~(1u << SBIT_LAT); // the pin_lo(SBIT_LAT) that ended the old loop
            }

            render_line = (render_line + 1) % NUM_ROWS;
            set_row_address(render_line);
        }
    }

    pin_lo(SBIT_OE);
    send_clocks(1);
}



/* ---- SDR init (software, before the peripheral takes the pins) --------- */

/*
 * Enter single-edge mode. The datasheet wants exactly 15 clock edges while
 * LAT is high, the last of them falling. Starting from CLK high that is seven
 * full falling/rising pulses plus one closing falling edge, which also leaves
 * the clock low for the peripheral to take over cleanly.
 *
 * This is the only command that samples both clock edges, so it cannot be
 * expressed as bus words and has to be bit-banged.
 */
static void send_sdr(void) {
    ESP_LOGI(TAG, "Sending SDR (enter single-edge mode)");

    gpio_set_level(PIN_CLK, 1);
    esp_rom_delay_us(2);
    gpio_set_level(PIN_LAT, 1);
    esp_rom_delay_us(2);

    // 7 pulses from the high level = 14 edges
    for (int i = 0; i < 7; i++) {
        gpio_set_level(PIN_CLK, 0);
        esp_rom_delay_us(2);
        gpio_set_level(PIN_CLK, 1);
        esp_rom_delay_us(2);
    }

    // edge 15, a falling one, leaving the clock low
    gpio_set_level(PIN_CLK, 0);
    esp_rom_delay_us(2);

    gpio_set_level(PIN_LAT, 0);
    esp_rom_delay_us(2);
}



/* ---- LCD_CAM + GDMA ---------------------------------------------------- */

/* dw0.size is 12 bits, so a descriptor covers at most 4095 bytes. Keep it a
 * multiple of 4 so every descriptor holds a whole number of bus words. */
#define DESC_MAX_LEN 4092
#define DESC_COUNT(bytes) (((bytes) + DESC_MAX_LEN - 1) / DESC_MAX_LEN)

#define INIT_BYTES (INIT_CLOCKS * sizeof(uint16_t))
#define FRAME_BYTES (FRAME_CLOCKS * sizeof(uint16_t))
#define INIT_DESCS DESC_COUNT(INIT_BYTES)
#define FRAME_DESCS DESC_COUNT(FRAME_BYTES)

static gdma_channel_handle_t dma_chan;
static dma_descriptor_t *init_desc;
static dma_descriptor_t *frame_desc[2];
static uint16_t *frame_buf[2];
static int front_buf;

/* Link n descriptors over one buffer. The tail points wherever the caller
 * wants the stream to continue, which is what makes the chain loop. */
static void desc_link(dma_descriptor_t *d, size_t n, void *buf, size_t bytes, dma_descriptor_t *tail_next, bool tail_eof) {
    uint8_t *p = buf;

    for (size_t i = 0; i < n; i++) {
        size_t len = bytes > DESC_MAX_LEN ? DESC_MAX_LEN : bytes;

        d[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        d[i].dw0.suc_eof = (i == n - 1) ? tail_eof : 0;
        d[i].dw0.size = len;
        d[i].dw0.length = len;
        d[i].buffer = p;
        d[i].next = (i + 1 < n) ? &d[i + 1] : tail_next;

        p += len;
        bytes -= len;
    }
}

static void lcd_init(void) {
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(1000);

    // PCLK = PLL_F160M / LCD_CLK_DIV, low in the first half cycle and idle low
    LCD_CAM.lcd_clock.lcd_clk_sel = 3;
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1;
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;
    LCD_CAM.lcd_clock.lcd_clkm_div_num = LCD_CLK_DIV;
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;

    // Plain 16-bit parallel output, no LCD framing of any kind
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0; // i8080 mode, not RGB
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0; // no RGB/YUV converter
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0; // do not auto-frame
    LCD_CAM.lcd_misc.lcd_bk_en = 1;
    LCD_CAM.lcd_data_dout_mode.val = 0; // no per-line data delays
    LCD_CAM.lcd_user.lcd_always_out_en = 1; // stream until stopped
    LCD_CAM.lcd_user.lcd_8bits_order = 0; // do not swap bytes
    LCD_CAM.lcd_user.lcd_bit_order = 0; // do not reverse bits
    LCD_CAM.lcd_user.lcd_2byte_en = 1; // 16-bit bus
    LCD_CAM.lcd_user.lcd_cmd = 0; // no command phase
    /* At least one dummy phase is needed for the DMA to trigger reliably. It
     * costs two clocks before the stream starts, which land ahead of the very
     * first vsync and are harmless. */
    LCD_CAM.lcd_user.lcd_dummy = 1;
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 1;

    // Route the bus and the clock out through the GPIO matrix
    for (int i = 0; i < 16; i++) {
        if (BUS_PINS[i] >= 0) {
            esp_rom_gpio_connect_out_signal(BUS_PINS[i], LCD_DATA_OUT0_IDX + i, false, false);
            gpio_set_drive_capability(BUS_PINS[i], GPIO_DRIVE_CAP_3);
        }
    }
    esp_rom_gpio_connect_out_signal(PIN_CLK, LCD_PCLK_IDX, false, false);
    gpio_set_drive_capability(PIN_CLK, GPIO_DRIVE_CAP_3);

    gdma_channel_alloc_config_t chan_cfg = { .direction = GDMA_CHANNEL_DIRECTION_TX };
    ESP_ERROR_CHECK(gdma_new_ahb_channel(&chan_cfg, &dma_chan));
    ESP_ERROR_CHECK(gdma_connect(dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0)));

    /* owner_check and auto_update_desc must both be off: otherwise the DMA
     * clears the owner bit as it goes and the chain stalls on its second lap
     * instead of looping forever. */
    gdma_strategy_config_t strategy = { .owner_check = false, .auto_update_desc = false };
    ESP_ERROR_CHECK(gdma_apply_strategy(dma_chan, &strategy));

    gdma_transfer_config_t transfer = { .max_data_burst_size = 32, .access_ext_mem = false };
    ESP_ERROR_CHECK(gdma_config_transfer(dma_chan, &transfer));
}

static void lcd_start(void) {
    gdma_reset(dma_chan);
    esp_rom_delay_us(1000);

    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;

    gdma_start(dma_chan, (intptr_t)&init_desc[0]);
    esp_rom_delay_us(100); // let the FIFO prime before the peripheral runs
    LCD_CAM.lcd_user.lcd_start = 1;
}

/* Show the buffer that was last drawn into. Both chains are re-pointed, so it
 * does not matter which one the DMA is currently walking. */
static void panel_present(void) {
    int back = front_buf ^ 1;

    frame_desc[0][FRAME_DESCS - 1].next = &frame_desc[back][0];
    frame_desc[1][FRAME_DESCS - 1].next = &frame_desc[back][0];
    front_buf = back;
}



/* ---- main -------------------------------------------------------------- */

void app_main(void) {
    ESP_LOGI(TAG, "DP3364S panel %dx%d, DCLK %.2f MHz, %.1f frames/s",
             TOTAL_COLS, NUM_ROWS * 2, DCLK_HZ / 1e6, (double) DCLK_HZ / FRAME_CLOCKS);
    ESP_LOGI(TAG, "init %u B (%u desc), frame %u B x2 (%u desc each)",
             (unsigned) INIT_BYTES, (unsigned) INIT_DESCS, (unsigned) FRAME_BYTES, (unsigned) FRAME_DESCS);

    uint16_t *init_buf = heap_caps_malloc(INIT_BYTES, MALLOC_CAP_DMA);
    frame_buf[0] = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_DMA);
    frame_buf[1] = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_DMA);
    init_desc = heap_caps_malloc(INIT_DESCS * sizeof(dma_descriptor_t), MALLOC_CAP_DMA);
    frame_desc[0] = heap_caps_malloc(FRAME_DESCS * sizeof(dma_descriptor_t), MALLOC_CAP_DMA);
    frame_desc[1] = heap_caps_malloc(FRAME_DESCS * sizeof(dma_descriptor_t), MALLOC_CAP_DMA);

    /* The back buffer is optional: without it the panel still runs, drawing
     * just has to go straight into the live frame and may tear. */
    if (!frame_buf[1] || !frame_desc[1]) {
        ESP_LOGW(TAG, "no room for a back buffer, running single-buffered");
        heap_caps_free(frame_buf[1]);
        heap_caps_free(frame_desc[1]);
        frame_buf[1] = frame_buf[0];
        frame_desc[1] = frame_desc[0];
    }

    if (!init_buf || !frame_buf[0] || !init_desc || !frame_desc[0]) {
        ESP_LOGE(TAG, "out of DMA-capable memory (need %u KiB, %u KiB free)",
                 (unsigned)((INIT_BYTES + FRAME_BYTES) / 1024),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DMA) / 1024));
        return;
    }

    /* Phase 1 first, so the display frame inherits the signal state the
     * register writes leave behind. The frame is built twice: the first pass
     * only warms up the carried state so the second one loops seamlessly. */
    build_hue_lut();
    draw_frame();

    build_register_init(init_buf);
    build_display_frame(frame_buf[0]);
    build_display_frame(frame_buf[0]);
    front_buf = 0;

    if (frame_buf[1] != frame_buf[0]) {
        memcpy(frame_buf[1], frame_buf[0], FRAME_BYTES);
    }

    /* Init runs once and falls through into the frame, which then loops on
     * itself - the register blocks are never reached again. */
    desc_link(init_desc, INIT_DESCS, init_buf, INIT_BYTES, &frame_desc[0][0], false);
    desc_link(frame_desc[0], FRAME_DESCS, frame_buf[0], FRAME_BYTES, &frame_desc[0][0], true);

    if (frame_desc[1] != frame_desc[0]) {
        desc_link(frame_desc[1], FRAME_DESCS, frame_buf[1], FRAME_BYTES, &frame_desc[0][0], true);
    }

    /* Drive every line low, then run the SDR sequence while the pins are still
     * plain GPIOs. The peripheral takes them over immediately afterwards. */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_CLK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    for (int i = 0; i < 16; i++) {
        if (BUS_PINS[i] >= 0) {
            io.pin_bit_mask |= 1ULL << BUS_PINS[i];
        }
    }
    ESP_ERROR_CHECK(gpio_config(&io));

    for (int i = 0; i < 16; i++) {
        if (BUS_PINS[i] >= 0) {
            gpio_set_level(BUS_PINS[i], 0);
        }
    }
    gpio_set_level(PIN_CLK, 0);
    esp_rom_delay_us(100);

    send_sdr();

    lcd_init();
    lcd_start();

    ESP_LOGI(TAG, "running - registers written once, frame looping in hardware");

    /*
     * Animation. The DMA keeps replaying the front buffer on its own, so the
     * loop only has to draw the next frame into the back buffer and flip:
     *
     *   1. change whatever get_pattern() reads,
     *   2. build_display_frame() into the buffer that is NOT on screen,
     *   3. panel_present() to point both chains at it.
     *
     * The flip only takes effect when the DMA reaches the end of the frame it
     * is currently playing, so a half-drawn buffer is never shown. Waiting one
     * frame period afterwards makes sure the DMA has actually moved across
     * before the next draw starts writing into the buffer it just left.
     */
    int64_t last_report = esp_timer_get_time();
    int frames = 0;
    uint32_t build_us = 0;

    while (1) {
        int64_t t0 = esp_timer_get_time();

        hue_phase = (hue_phase + 1) & (HUE_STEPS - 1);

        draw_frame();                                   // fill the framebuffer
        build_display_frame(frame_buf[front_buf ^ 1]);  // encode it for the panel
        panel_present();                                // show it

        build_us = (uint32_t)(esp_timer_get_time() - t0);
        frames++;

        /* At the default 100 Hz tick pdMS_TO_TICKS(7) rounds down to 0, and
         * vTaskDelay(0) only yields - the idle task never gets to run and the
         * task watchdog eventually fires. Always block for a whole tick. */
        TickType_t wait = pdMS_TO_TICKS(FRAME_PERIOD_MS);
        vTaskDelay(wait ? wait : 1);

        if (esp_timer_get_time() - last_report >= 5000000) {
            ESP_LOGI(TAG, "%.1f fps drawn, %u us per frame build", frames * 1e6 / (double)(esp_timer_get_time() - last_report), (unsigned) build_us);
            last_report = esp_timer_get_time();
            frames = 0;
        }
    }

}
