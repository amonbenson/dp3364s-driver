#pragma once

#include <stdint.h>

typedef void (*gfx_set_pixel_fn)(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

typedef struct {
    int16_t x;
    int16_t y;
} gfx_point_t;

typedef struct {
    int16_t width;
    int16_t height;
} gfx_size_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} gfx_rect_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} gfx_color_t;

typedef struct {
    gfx_size_t size;
    gfx_set_pixel_fn set_pixel;
} gfx_prim_context_t;

void gfx_draw_point(const gfx_prim_context_t *ctx, const gfx_point_t p, const gfx_color_t color);

void gfx_draw_line(const gfx_prim_context_t *ctx, const gfx_point_t p1, const gfx_color_t color1, const gfx_point_t p2, const gfx_color_t color2);

void gfx_draw_rect(const gfx_prim_context_t *ctx, const gfx_rect_t rect, const gfx_color_t color);
