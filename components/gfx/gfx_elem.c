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

void gfx_elem_create(gfx_elem_context_t *ctx, gfx_elem_t *elem, gfx_elem_callbacks_t callbacks, gfx_appearance_t appearance) {
    elem->callbacks = callbacks;
    elem->appearance = appearance;

    elem->minimum_size = (gfx_size_t){ 0, 0 };
    elem->grow_portion = (gfx_size_t){ 0, 0 };
    elem->computed_bounds = (gfx_rect_t) { 0, 0, 0, 0 };

    elem->parent = NULL;
    elem->children = NULL;
    elem->next_sibling = NULL;

    // Invoke the init callback if provided
    if (elem->callbacks.init) {
        elem->callbacks.init(ctx, elem);
    }
}

void gfx_elem_update(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    // Update children first so a parent can derive its own minimum size from theirs
    gfx_elem_t *child = elem->children;
    while (child) {
        gfx_elem_update(ctx, child);
        child = child->next_sibling;
    }

    // Invoke the update callback: leaf elements set minimum_size explicitly, other
    // elements (e.g. containers) derive it from their now up-to-date children
    if (elem->callbacks.update) {
        elem->callbacks.update(ctx, elem);
    }

    // The root has no parent to arrange it, so it spans the whole screen and kicks off
    // the arrange pass for the entire hierarchy
    if (!elem->parent) {
        elem->computed_bounds = (gfx_rect_t){ 0, 0, ctx->prim_ctx->size.width, ctx->prim_ctx->size.height };
        gfx_elem_arrange(ctx, elem);
    }
}

void gfx_elem_arrange(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    // How (and whether) to position children is algorithm-specific - a container packs
    // and grows them along an axis, a future scrolling/relative element might do
    // something else entirely - so it's left to the element's own arrange callback,
    // which is also responsible for recursing into gfx_elem_arrange() for its children.
    if (elem->callbacks.arrange) {
        elem->callbacks.arrange(ctx, elem);
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

    // Render all children on top of the current element
    gfx_elem_t *child = elem->children;
    while (child) {
        gfx_elem_render(ctx, child);
        child = child->next_sibling;
    }
}
