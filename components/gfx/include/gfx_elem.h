#pragma once

#include <stdbool.h>
#include "gfx_prim.h"

#define GFX_ELEM_DEBUG_BOUNDS



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
    const gfx_prim_context_t *prim_ctx;
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

typedef enum {
    GFX_ALIGNMENT_START = 0,
    GFX_ALIGNMENT_CENTER,
    GFX_ALIGNMENT_END,
} gfx_alignment_t;



typedef struct gfx_elem_t gfx_elem_t;

typedef void (*gfx_elem_init_cb)(gfx_elem_context_t *ctx, gfx_elem_t *elem);
typedef void (*gfx_elem_update_cb)(gfx_elem_context_t *ctx, gfx_elem_t *elem);
typedef void (*gfx_elem_arrange_cb)(gfx_elem_context_t *ctx, gfx_elem_t *elem);
typedef void (*gfx_elem_render_cb)(gfx_elem_context_t *ctx, gfx_elem_t *elem);

typedef struct {
    gfx_elem_init_cb init;
    gfx_elem_update_cb update;
    gfx_elem_arrange_cb arrange;
    gfx_elem_render_cb render;
} gfx_elem_callbacks_t;

struct gfx_elem_t {
    gfx_elem_callbacks_t callbacks;
    gfx_appearance_t appearance;

    gfx_size_t minimum_size;
    gfx_size_t grow_portion;
    gfx_rect_t computed_bounds;

    gfx_elem_t *parent;
    gfx_elem_t *children;
    gfx_elem_t *next_sibling;
};

gfx_color_t gfx_get_appearance_color(const gfx_elem_context_t *ctx, gfx_appearance_t appearance);

void gfx_elem_create(gfx_elem_context_t *ctx, gfx_elem_t *elem, gfx_elem_callbacks_t callbacks, gfx_appearance_t appearance);

void gfx_elem_update(gfx_elem_context_t *ctx, gfx_elem_t *elem);
void gfx_elem_arrange(gfx_elem_context_t *ctx, gfx_elem_t *elem);

void gfx_elem_render(gfx_elem_context_t *ctx, gfx_elem_t *elem);
