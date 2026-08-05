#include "widgets/text.h"

#include <stddef.h>

static void gfx_text_update(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_text_t *text = (gfx_text_t *) elem;

    // A text widget is a leaf whose size comes from measuring its own string
    // against its font, and which never grows to fill extra space.
    elem->minimum_size = text->config.font ? gfx_measure_text(text->config.font, text->config.text) : (gfx_size_t) { 0, 0 };
    elem->grow = (gfx_size_t) { 0, 0 };
    elem->appearance = text->config.appearance;
}

static void gfx_text_render(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_text_t *text = (gfx_text_t *) elem;
    if (!text->config.font) {
        return;
    }

    gfx_color_t color = gfx_get_appearance_color(ctx, elem->appearance);
    gfx_point_t origin = { elem->computed_bounds.x, elem->computed_bounds.y };
    gfx_draw_text(ctx->prim_ctx, origin, text->config.font, text->config.text, color);
}

void gfx_text_create(gfx_elem_context_t *ctx, gfx_text_t *elem, const gfx_text_config_t *config) {
    elem->config = *config;

    gfx_elem_callbacks_t callbacks = {
        .init = NULL,
        .update = gfx_text_update,
        .arrange = NULL,
        .render = gfx_text_render,
    };

    gfx_elem_create(ctx, &elem->base, callbacks);
}
