#include "widgets/container.h"

#include <stdbool.h>
#include <stddef.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static void gfx_container_update(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_container_t *container = (gfx_container_t *) elem;
    bool horizontal = container->config.direction == GFX_DIRECTION_HORIZONTAL;

    // Derive our own minimum size from the (already updated) children's minimum sizes:
    // summed along the content direction (plus a gap between each pair of children),
    // maxed across the cross direction.
    gfx_size_t minimum_size = { 0, 0 };
    int child_count = 0;

    gfx_elem_t *child = elem->children;
    while (child) {
        if (horizontal) {
            minimum_size.width += child->minimum_size.width;
            minimum_size.height = MAX(minimum_size.height, child->minimum_size.height);
        } else {
            minimum_size.width = MAX(minimum_size.width, child->minimum_size.width);
            minimum_size.height += child->minimum_size.height;
        }
        child_count++;
        child = child->next_sibling;
    }

    int gap_total = child_count > 1 ? (child_count - 1) * ctx->theme.spacing : 0;
    if (horizontal) {
        minimum_size.width += gap_total;
    } else {
        minimum_size.height += gap_total;
    }

    elem->minimum_size = minimum_size;
    elem->grow = container->config.grow;
}

static void gfx_container_arrange(gfx_elem_context_t *ctx, gfx_elem_t *elem) {
    gfx_container_t *container = (gfx_container_t *) elem;
    bool horizontal = container->config.direction == GFX_DIRECTION_HORIZONTAL;
    int spacing = ctx->theme.spacing;

    // Sum the minimum space required (including gaps between children) and the total
    // grow weight along the content direction
    uint16_t used = 0;
    int32_t total_grow = 0;
    int child_count = 0;

    gfx_elem_t *child = elem->children;
    while (child) {
        used += horizontal ? child->minimum_size.width : child->minimum_size.height;
        total_grow += horizontal ? child->grow.width : child->grow.height;
        child_count++;
        child = child->next_sibling;
    }
    used += child_count > 1 ? (uint16_t) ((child_count - 1) * spacing) : 0;

    // Divide the remaining space amongst all growable children, weighted by grow
    uint16_t available = horizontal ? elem->computed_bounds.width : elem->computed_bounds.height;
    uint16_t extra = used < available ? (available - used) : 0;

    // Any space left over once growable children have taken their share is distributed
    // before/after the children according to justification, rather than always sitting
    // at the end (the old, implicitly START-justified behavior).
    uint16_t consumed = child_count > 1 ? (uint16_t) ((child_count - 1) * spacing) : 0;
    child = elem->children;
    while (child) {
        int32_t child_grow = horizontal ? child->grow.width : child->grow.height;
        uint16_t child_share = total_grow > 0 ? (uint16_t) ((uint32_t) extra * child_grow / total_grow) : 0;
        consumed += (horizontal ? child->minimum_size.width : child->minimum_size.height) + child_share;
        child = child->next_sibling;
    }
    uint16_t leftover = available > consumed ? (available - consumed) : 0;

    uint16_t offset;
    switch (container->config.justification) {
        case GFX_PLACEMENT_CENTER:
            offset = leftover / 2;
            break;
        case GFX_PLACEMENT_END:
            offset = leftover;
            break;
        case GFX_PLACEMENT_START:
        default:
            offset = 0;
            break;
    }

    // Lay out each child along the content direction, spaced by a gap between each pair
    // of children. A child that can grow along the cross axis stretches across it as
    // before; otherwise it keeps its minimum cross size and is positioned within the
    // cross axis according to alignment.
    child = elem->children;
    while (child) {
        int32_t child_grow = horizontal ? child->grow.width : child->grow.height;
        uint16_t child_share = total_grow > 0 ? (uint16_t) ((uint32_t) extra * child_grow / total_grow) : 0;
        uint16_t child_length = (horizontal ? child->minimum_size.width : child->minimum_size.height) + child_share;

        int32_t cross_grow = horizontal ? child->grow.height : child->grow.width;
        uint16_t cross_available = horizontal ? elem->computed_bounds.height : elem->computed_bounds.width;
        uint16_t cross_min = horizontal ? child->minimum_size.height : child->minimum_size.width;

        uint16_t cross_length = cross_available;
        uint16_t cross_offset = 0;
        if (cross_grow <= 0) {
            cross_length = cross_min < cross_available ? cross_min : cross_available;
            switch (container->config.alignment) {
                case GFX_PLACEMENT_CENTER:
                    cross_offset = (cross_available - cross_length) / 2;
                    break;
                case GFX_PLACEMENT_END:
                    cross_offset = cross_available - cross_length;
                    break;
                case GFX_PLACEMENT_START:
                default:
                    cross_offset = 0;
                    break;
            }
        }

        gfx_rect_t child_bounds;
        if (horizontal) {
            child_bounds = (gfx_rect_t) {
                .x = elem->computed_bounds.x + offset,
                .y = elem->computed_bounds.y + cross_offset,
                .width = child_length,
                .height = cross_length,
            };
        } else {
            child_bounds = (gfx_rect_t) {
                .x = elem->computed_bounds.x + cross_offset,
                .y = elem->computed_bounds.y + offset,
                .width = cross_length,
                .height = child_length,
            };
        }

        child->computed_bounds = child_bounds;
        gfx_elem_arrange(ctx, child);

        offset += child_length + spacing;
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

    gfx_elem_create(ctx, &elem->base, callbacks);
}
