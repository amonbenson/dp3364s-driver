#include "gfx_prim.h"

#include <stdint.h>
#include <stdbool.h>

static inline int gfx_iabs(int v) {
    return v < 0 ? -v : v;
}

void gfx_draw_point(const gfx_context_t *ctx, const gfx_point_t p, const gfx_color_t c) {
    if ((unsigned )p.x >= (unsigned) ctx->width || (unsigned) p.y >= (unsigned) ctx->height) {
        return;
    }

    ctx->set_pixel(p.x, p.y, c.r, c.g, c.b);
}

/* Bresenham's line algorithm, plotting the same pixel count on every octant,
 * with the colour linearly interpolated between the endpoints. steps is the
 * number of pixels beyond the first, so dividing by it (once per channel,
 * not per pixel) gives a fixed-point 16.16 step that is just added each
 * iteration - no per-pixel division or float math. */
void gfx_draw_line(const gfx_context_t *ctx, const gfx_point_t p0, const gfx_color_t c1, const gfx_point_t p1, const gfx_color_t c2) {
    int dx = gfx_iabs(p1.x - p0.x);
    int dy = -gfx_iabs(p1.y - p0.y);
    int sx = (p0.x < p1.x) ? 1 : -1;
    int sy = (p0.y < p1.y) ? 1 : -1;
    int err = dx + dy;
    int steps = (dx > -dy) ? dx : -dy;

    int32_t r = (int32_t)c1.r << 16, g = (int32_t)c1.g << 16, b = (int32_t)c1.b << 16;
    int32_t dr = steps ? (((int32_t)c2.r - c1.r) << 16) / steps : 0;
    int32_t dg = steps ? (((int32_t)c2.g - c1.g) << 16) / steps : 0;
    int32_t db = steps ? (((int32_t)c2.b - c1.b) << 16) / steps : 0;

    int x = p0.x, y = p0.y;

    while (true) {
        const gfx_color_t c = {
            .r = (uint8_t)((r + 0x8000) >> 16),
            .g = (uint8_t)((g + 0x8000) >> 16),
            .b = (uint8_t)((b + 0x8000) >> 16),
        };
        gfx_draw_point(ctx, (gfx_point_t) { x, y }, c);

        if (x == p1.x && y == p1.y) {
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

        r += dr;
        g += dg;
        b += db;
    }
}
