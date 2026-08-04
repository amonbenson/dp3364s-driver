#include "gfx_elem.h"

#include <stddef.h>
#include "gfx_prim.h"

gfx_color_t gfx_get_appearance_color(const gfx_elem_context_t *ctx, gfx_appearance_t appearance) {
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

void gfx_elem_create(gfx_elem_context_t *ctx, gfx_elem_t *elem, gfx_elem_callbacks_t callbacks, gfx_appearance_t appearance, gfx_alignment_t alignment, gfx_size_t preferred_size) {
    elem->callbacks = callbacks;
    elem->appearance = appearance;
    elem->alignment = alignment;
    elem->preferred_size = preferred_size;

    elem->computed_bounds = (gfx_rect_t) { 0, 0, preferred_size.width, preferred_size.height };
    elem->parent = NULL;
    elem->children = NULL;
    elem->next_sibling = NULL;

    // Invoke the init callback if provided
    if (elem->callbacks.init) {
        elem->callbacks.init(ctx, elem);
    }
}

void gfx_elem_render(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    if (elem->callbacks.render) {
        elem->callbacks.render(ctx, elem);
    }

#ifdef GFX_ELEM_DEBUG_BOUNDS
    // Render the bounds of the element (for debugging purposes, can be removed in production)
    gfx_color_t debug_color = { 255, 0, 0 };
    gfx_draw_rect(ctx->prim_ctx, elem->computed_bounds, debug_color);
#endif

    // Render all children
    gfx_elem_t *child = elem->children;
    while (child) {
        gfx_elem_render(ctx, child);
        child = child->next_sibling;
    }
}
