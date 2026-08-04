#pragma once

#include "gfx_prim.h"

typedef struct {
    int spacing;
    struct {
        gfx_color_t primary;
        gfx_color_t secondary;
        gfx_color_t accent;
    } colors;
} gfx_theme_t;

#define GFX_THEME_DEFAULT (gfx_theme_t) { \
    .spacing = 1, \
    .colors = { \
        .primary = { .r = 255, .g = 255, .b = 255 }, \
        .secondary = { .r = 128, .g = 128, .b = 128 }, \
        .accent = { .r = 255, .g = 128, .b = 0 }, \
    } \
}



typedef struct {
    gfx_prim_context_t *prim_ctx;
    gfx_theme_t theme;
} gfx_elem_context_t;

typedef enum {
    GFX_APPEARANCE_PRIMARY = 0,
    GFX_APPEARANCE_SECONDARY,
    GFX_APPEARANCE_ACCENT,
} gfx_appearance_t;

typedef enum {
    GFX_DIRECTION_HORIZONTAL = 0,
    GFX_DIRECTION_VERTICAL,
} gfx_direction_t;


typedef struct gfx_elem_t gfx_elem_t;

typedef void (*gfx_elem_init_cb)(gfx_elem_context_t *ctx, gfx_elem_t *elem);
typedef void (*gfx_elem_render_cb)(gfx_elem_context_t *ctx, gfx_elem_t *elem);

typedef struct {
    gfx_elem_init_cb init;
    gfx_elem_render_cb render;
} gfx_elem_callbacks_t;

struct gfx_elem_t {
    gfx_elem_callbacks_t callbacks;
    gfx_appearance_t appearance;
    gfx_rect_t bounds;
};

void gfx_elem_create(gfx_elem_context_t *ctx, gfx_elem_t *elem, gfx_elem_callbacks_t callbacks, gfx_appearance_t appearance, gfx_rect_t bounds);
void gfx_elem_render(gfx_elem_context_t *ctx, gfx_elem_t *elem);



typedef struct {
    gfx_appearance_t appearance;
    gfx_direction_t direction;
    int16_t length;
} gfx_separator_config_t;

typedef struct {
    gfx_elem_t base;
    gfx_separator_config_t config;
} gfx_separator_t;

void gfx_separator_create(gfx_elem_context_t *ctx, gfx_separator_t *elem, const gfx_separator_config_t *config, int16_t x, int16_t y);
