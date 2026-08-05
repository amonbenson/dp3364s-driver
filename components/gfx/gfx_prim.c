#include "gfx_prim.h"

#include <stdint.h>
#include <stdbool.h>

static inline int gfx_iabs(int v) {
    return v < 0 ? -v : v;
}

void gfx_draw_point(const gfx_prim_context_t *ctx, const gfx_point_t p, const gfx_color_t color) {
    if ((unsigned) p.x >= (unsigned) ctx->size.width || (unsigned) p.y >= (unsigned) ctx->size.height) {
        return;
    }

    ctx->set_pixel(p.x, p.y, color.r, color.g, color.b);
}

/* Bresenham's line algorithm, plotting the same pixel count on every octant,
 * with the colour linearly interpolated between the endpoints. steps is the
 * number of pixels beyond the first, so dividing by it (once per channel,
 * not per pixel) gives a fixed-point 16.16 step that is just added each
 * iteration - no per-pixel division or float math. */
void gfx_draw_line(const gfx_prim_context_t *ctx, const gfx_point_t p1, const gfx_point_t p2, const gfx_color_t color) {
    int dx = gfx_iabs(p2.x - p1.x);
    int dy = -gfx_iabs(p2.y - p1.y);
    int sx = (p1.x < p2.x) ? 1 : -1;
    int sy = (p1.y < p2.y) ? 1 : -1;
    int err = dx + dy;

    int x = p1.x, y = p1.y;

    while (true) {
        gfx_draw_point(ctx, (gfx_point_t) { x, y }, color);

        if (x == p2.x && y == p2.y) {
            break;
        }

        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

void gfx_draw_rect(const gfx_prim_context_t *ctx, const gfx_rect_t rect, const gfx_color_t c) {
    gfx_point_t top_left = { .x = rect.x, .y = rect.y };
    gfx_point_t top_right = { .x = rect.x + rect.width - 1, .y = rect.y };
    gfx_point_t bottom_left = { .x = rect.x, .y = rect.y + rect.height - 1 };
    gfx_point_t bottom_right = { .x = rect.x + rect.width - 1, .y = rect.y + rect.height - 1 };

    gfx_draw_line(ctx, top_left, top_right, c);
    gfx_draw_line(ctx, top_right, bottom_right, c);
    gfx_draw_line(ctx, bottom_right, bottom_left, c);
    gfx_draw_line(ctx, bottom_left, top_left, c);
}
