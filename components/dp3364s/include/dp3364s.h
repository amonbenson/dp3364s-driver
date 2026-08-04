/*
 * dp3364s.h - Driver for RGB LED matrix panels built from DP3364S column
 * driver chips and a binary row decoder (e.g. TC7262).
 *
 * Once dp3364s_init() returns, the panel is refreshed entirely by hardware: a
 * GDMA descriptor chain replays the presented frame with no CPU involvement.
 * Callers draw into a plain (x, y) RGB framebuffer with dp3364s_set_pixel()
 * and call dp3364s_update() once per frame to encode and present it.
 *
 * Chip registers, bit serialisation, scan order and signal timing are
 * entirely internal to this driver - callers only ever deal in pixels.
 * Wiring, panel geometry and clock timing are fixed at compile time in
 * dp3364s.c to match a specific physical panel and must be edited there if
 * the hardware changes.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#define DP3364S_WIDTH 128
#define DP3364S_HEIGHT 64

/* Bring up the LCD_CAM/GDMA hardware and run the panel's register/command
 * init sequence. Allocates DMA-capable buffers and starts the panel scanning
 * out whatever is currently in the framebuffer (black, until the first
 * dp3364s_set_pixel() calls). Call exactly once, before dp3364s_set_pixel() or
 * dp3364s_update(). Returns ESP_ERR_NO_MEM if DMA-capable memory ran out. */
esp_err_t dp3364s_init(void);

/* Write one pixel into the framebuffer, 0..255 per channel, gamma-corrected
 * internally. Takes effect on the next dp3364s_update() call. Out-of-range
 * coordinates are ignored. */
void dp3364s_set_pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

/* Clear the framebuffer to black. Takes effect on the next dp3364s_update()
 * call. */
void dp3364s_clear(void);

/* Encode the framebuffer and present it: the panel shows the new frame at
 * its next refresh, tearing-free. Call once per drawn frame. */
void dp3364s_update(void);

/* Time in milliseconds the DMA needs to play one full frame. Callers should
 * not call dp3364s_update() faster than this, since the encode buffer not
 * currently on screen is reused for the following frame. */
uint32_t dp3364s_frame_period_ms(void);
