#include "widgets/separator.h"

#include <stddef.h>

static void gfx_separator_render(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_separator_t *separator = (gfx_separator_t *) elem;

    // Draw either a horizontal or vertical line based on the separator's direction
    gfx_point_t start, end;
    if (separator->config.direction == GFX_DIRECTION_HORIZONTAL) {
        start = (gfx_point_t) { elem->computed_bounds.x, elem->computed_bounds.y + elem->computed_bounds.height / 2 };
        end = (gfx_point_t) { elem->computed_bounds.x + elem->computed_bounds.width - 1, start.y };
    } else {
        start = (gfx_point_t) { elem->computed_bounds.x + elem->computed_bounds.width / 2, elem->computed_bounds.y };
        end = (gfx_point_t) { start.x, elem->computed_bounds.y + elem->computed_bounds.height - 1 };
    }

    // Get the color based on the appearance and draw the line
    gfx_color_t color = gfx_get_appearance_color(ctx, elem->appearance);
    gfx_draw_line(ctx->prim_ctx, start, color, end, color);
}

void gfx_separator_create(gfx_elem_context_t *ctx, gfx_separator_t *elem, const gfx_separator_config_t *config) {
    elem->config = *config;

    gfx_size_t preferred_size = {
        .width = config->direction == GFX_DIRECTION_HORIZONTAL ? config->length : 1,
        .height = config->direction == GFX_DIRECTION_VERTICAL ? config->length : 1
    };
    gfx_elem_callbacks_t callbacks = {
        .init = NULL,
        .render = gfx_separator_render
    };

    gfx_elem_create(ctx, &elem->base, callbacks, GFX_APPEARANCE_PRIMARY, config->alignment, preferred_size);
}
