# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Proof-of-concept ESP-IDF (ESP32-S3) firmware that drives a 128x64 RGB LED matrix
built from **DP3364S** column driver chips (PWM/SRAM) and TC7262 binary row
decoders. These panels look like HUB75 but are not: the DP3364S holds the
frame in its own SRAM and needs a config-register write sequence plus a
specific command handshake before it displays anything, and the chip is
barely documented (the only datasheet is linked in a comment in `main/main.c`
next to `REGS`).

Almost all of the substance is in the single file `main/main.c`, which is
heavily commented with the *why* of the protocol/hardware tricks used. Read
its top-of-file comment block and the section banners (`/* ---- ... ---- */`)
before making changes — they explain the signal timing invariants that are
easy to break silently.

## Build / flash / monitor

Requires the ESP-IDF toolchain (`idf.py` on PATH, target already set to esp32s3
in `sdkconfig`).

```
idf.py set-target esp32s3   # only needed once / when switching targets
idf.py build
idf.py flash
idf.py monitor
idf.py build flash monitor  # all three
```

There is no test suite, linter, or CI in this repo — it's a single-file
firmware PoC. Verifying a change means building it and, ideally, observing the
panel or a logic analyzer capture (see `Logic Capture.sal`, a Saleae capture
file used during protocol reverse-engineering).

## Architecture

**Signal generation is entirely in hardware after startup.** The LCD_CAM
peripheral is configured as a 16-bit parallel i8080 output port (not RGB LCD
mode): its PCLK pin drives the panel clock (an exact divider of 160 MHz, set
by `LCD_CLK_DIV`), and the other 13 bus bits are GPIO-routed panel signals
(R1/G1/B1/R2/G2/B2, row address A-E, LAT, OE — see the `SBIT_*` enum and
`BUS_PINS` table). GDMA feeds the peripheral from a descriptor chain that
loops back on itself, so once started the CPU never touches a pin again.

**Two-phase descriptor chain:**
1. **Init phase** (`build_register_init` → `init_desc`): writes every chip
   config register in `REGS[]` once, encoded as bus words via the
   `send_vsync` / `send_latches` / `send_to_allRGB` stream-builder helpers.
2. **Display phase** (`build_display_frame` → `frame_desc[0]`/`frame_desc[1]`):
   one frame of pixel data, looping on itself forever.

The last descriptor of the init chain points into the display chain; the last
descriptor of the display chain points back to its own head. This means the
register writes happen exactly once at boot and the frame then repeats
indefinitely with zero CPU involvement — `app_main`'s main loop only redraws
into the *back* buffer and calls `panel_present()` to flip which chain the
next loop will point at (double-buffered to avoid tearing; `frame_buf[1]`
gracefully degrades to single-buffered if the second DMA-capable allocation
fails).

**Two independent representations, one conversion function:**
- `framebuffer[y][x][3]` — a plain (x, y) RGB buffer, values already passed
  through `gamma_lut` (13-bit PWM duty, `GAMMA` = 2.2). `draw_frame()` is the
  *only* place that decides what's drawn; it knows nothing about scan order,
  chip chaining, or bit serialization. **This is the function to replace to
  display something else.**
- `build_display_frame()` is pure protocol: it walks rows/channels/chips in
  panel scan order and serializes each pixel pair MSB-first into 16-bit bus
  words. It is the only performance-critical loop (runs `DATA_CLOCKS` times
  per frame) and depends on the six data lines staying on bus bits 0..5 in
  RGB1/RGB2 order (enforced by a `_Static_assert` on the `SBIT_*` values).

**One protocol exception:** the SDR command (enter single-edge mode) samples
both clock edges, so it can't be expressed as bus words through the LCD_CAM
peripheral. `send_sdr()` bit-bangs it directly on the GPIOs *before*
`lcd_init()` hands the pins to the peripheral via the GPIO matrix
(`esp_rom_gpio_connect_out_signal`).

**Panel geometry constants** (`NUM_CHANNELS`, `NUM_ROWS`, `CHIPS_PER_CHAIN`,
`TOTAL_COLS`) and the **wiring table** (`PIN_*` enum at the top of the file)
must match the physical panel/wiring; GPIOs 1-14 were chosen to avoid flash/
PSRAM pins (26-37), USB (19-20), and strapping pins (0, 45, 46) on the S3.

## Gotchas worth knowing before editing

- Changing `LCD_CLK_DIV` trades panel clock speed for frame rate/flicker
  margin — useful for slowing things down during debugging but not a free
  parameter in production.
- `gdma_strategy_config_t` must keep `owner_check = false` and
  `auto_update_desc = false`, otherwise the DMA clears descriptor owner bits
  as it passes and the self-looping chain stalls after one lap.
- `build_display_frame()` is called twice at startup before the panel starts
  (once to let carried-over signal state settle, once for the real first
  frame) — this is intentional, not a bug.
