#include "widgets/container.h"

#include <stdbool.h>
#include <stddef.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static void gfx_container_update(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_container_t *container = (gfx_container_t *) elem;
    bool horizontal = container->config.direction == GFX_DIRECTION_HORIZONTAL;

    // Derive our own minimum size from the (already updated) children's minimum sizes:
    // summed along the content direction, maxed across the cross direction.
    gfx_size_t minimum_size = { 0, 0 };
    gfx_elem_t *child = elem->children;
    while (child) {
        if (horizontal) {
            minimum_size.width += child->minimum_size.width;
            minimum_size.height = MAX(minimum_size.height, child->minimum_size.height);
        } else {
            minimum_size.width = MAX(minimum_size.width, child->minimum_size.width);
            minimum_size.height += child->minimum_size.height;
        }
        child = child->next_sibling;
    }

    elem->minimum_size = minimum_size;
}

static void gfx_container_arrange(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_container_t *container = (gfx_container_t *) elem;
    bool horizontal = container->config.direction == GFX_DIRECTION_HORIZONTAL;

    // Sum the minimum space required and the total grow weight along the content direction
    uint16_t used = 0;
    int32_t total_grow = 0;

    gfx_elem_t *child = elem->children;
    while (child) {
        used += horizontal ? child->minimum_size.width : child->minimum_size.height;
        total_grow += horizontal ? child->grow_portion.width : child->grow_portion.height;
        child = child->next_sibling;
    }

    // Divide the remaining space amongst all growable children, weighted by grow_portion
    uint16_t available = horizontal ? elem->computed_bounds.width : elem->computed_bounds.height;
    uint16_t extra = used < available ? (available - used) : 0;

    // Lay out each child along the content direction, stretching it across the cross axis
    uint16_t offset = 0;
    child = elem->children;
    while (child) {
        int32_t child_grow = horizontal ? child->grow_portion.width : child->grow_portion.height;
        uint16_t child_share = total_grow > 0 ? (uint16_t) ((uint32_t) extra * child_grow / total_grow) : 0;
        uint16_t child_length = (horizontal ? child->minimum_size.width : child->minimum_size.height) + child_share;

        gfx_rect_t child_bounds;
        if (horizontal) {
            child_bounds = (gfx_rect_t) {
                .x = elem->computed_bounds.x + offset,
                .y = elem->computed_bounds.y,
                .width = child_length,
                .height = elem->computed_bounds.height,
            };
        } else {
            child_bounds = (gfx_rect_t) {
                .x = elem->computed_bounds.x,
                .y = elem->computed_bounds.y + offset,
                .width = elem->computed_bounds.width,
                .height = child_length,
            };
        }

        child->computed_bounds = child_bounds;
        gfx_elem_arrange(ctx, child);

        offset += child_length;
        child = child->next_sibling;
    }
}

void gfx_container_create(gfx_elem_context_t *ctx, gfx_container_t *elem, const gfx_container_config_t *config) {
    elem->config = *config;

    gfx_elem_callbacks_t callbacks = {
        .init = NULL,
        .update = gfx_container_update,
        .arrange = gfx_container_arrange,
        .render = NULL,
    };

    gfx_elem_create(ctx, &elem->base, callbacks, config->appearance);
}
