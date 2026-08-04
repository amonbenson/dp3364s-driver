#include "gfx_elem.h"

#include <stddef.h>
#include "gfx_prim.h"



static gfx_color_t gfx_get_appearance_color(const gfx_elem_context_t *ctx, gfx_appearance_t appearance) {
    switch (appearance) {
        case GFX_APPEARANCE_PRIMARY:
            return ctx->theme.colors.primary;
        case GFX_APPEARANCE_SECONDARY:
            return ctx->theme.colors.secondary;
        case GFX_APPEARANCE_ACCENT:
            return ctx->theme.colors.accent;
        default:
            return (gfx_color_t){0, 0, 0}; // Default to black if unknown
    }
}



void gfx_elem_create(gfx_elem_context_t *ctx, gfx_elem_t *elem, gfx_elem_callbacks_t callbacks, gfx_appearance_t appearance, gfx_rect_t bounds) {
    elem->callbacks = callbacks;
    elem->appearance = appearance;
    elem->bounds = bounds;

    // Invoke the init callback if provided
    if (elem->callbacks.init) {
        elem->callbacks.init(ctx, elem);
    }
}

void gfx_elem_render(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    if (elem->callbacks.render) {
        elem->callbacks.render(ctx, elem);
    }
}



static void gfx_separator_render(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_color_t color = gfx_get_appearance_color(ctx, elem->appearance);
    gfx_point_t start = { .x = elem->bounds.x, .y = elem->bounds.y };
    gfx_point_t end = { .x = elem->bounds.x + elem->bounds.width - 1, .y = elem->bounds.y + elem->bounds.height - 1 };

    gfx_draw_line(ctx->prim_ctx, start, color, end, color);
}

void gfx_separator_create(gfx_elem_context_t *ctx, gfx_separator_t *elem, const gfx_separator_config_t *config, int16_t x, int16_t y) {
    elem->config = *config;

    gfx_rect_t bounds = {
        .x = x,
        .y = y,
        .width = config->direction == GFX_DIRECTION_HORIZONTAL ? config->length : 1,
        .height = config->direction == GFX_DIRECTION_VERTICAL ? config->length : 1
    };
    gfx_elem_callbacks_t callbacks = {
        .init = NULL,
        .render = gfx_separator_render
    };

    gfx_elem_create(ctx, &elem->base, callbacks, GFX_APPEARANCE_PRIMARY, bounds);
}
