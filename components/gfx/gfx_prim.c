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

void gfx_draw_text(const gfx_prim_context_t *ctx, const gfx_point_t p, const gfx_font_t *font, const char *text, const gfx_color_t color) {
    if (!font) {
        return;
    }

    int pen_x = p.x;
    int baseline_y = p.y + 2 * font->cap_height - font->ascent; // Adjust the baseline for the distance between cap height and ascent

    const char *s = text;
    while (*s) {
        uint32_t codepoint = gfx_utf8_next(&s);
        const gfx_glyph_t *glyph = gfx_font_find_glyph(font, codepoint);
        if (!glyph) {
            continue;
        }

        // BDF rows run top to bottom; per the BBX field the top row sits
        // (yoff + height - 1) pixels above the baseline.
        int top_y = baseline_y - (glyph->yoff + glyph->height - 1);
        int row_bytes = (glyph->width + 7) / 8;
        const uint8_t *rows = font->bitmap + glyph->bitmap_offset;

        for (int row = 0; row < glyph->height; row++) {
            const uint8_t *row_data = rows + row * row_bytes;
            for (int col = 0; col < glyph->width; col++) {
                bool set = (row_data[col / 8] >> (7 - (col % 8))) & 1;
                if (set) {
                    gfx_point_t px = { (int16_t) (pen_x + glyph->xoff + col), (int16_t) (top_y + row) };
                    gfx_draw_point(ctx, px, color);
                }
            }
        }

        pen_x += glyph->dwidth;
    }
}

gfx_size_t gfx_measure_text(const gfx_font_t *font, const char *text) {
    if (!font) {
        return (gfx_size_t) { 0, 0 };
    }

    int width = 0;

    const char *s = text;
    while (*s) {
        uint32_t codepoint = gfx_utf8_next(&s);
        const gfx_glyph_t *glyph = gfx_font_find_glyph(font, codepoint);
        if (glyph) {
            width += glyph->dwidth;
        }
    }

    // Every glyph's advance (DWIDTH) includes the gap to the next glyph, so we need to drop the last one
    if (width > 0) {
        width -= 1;
    }

    return (gfx_size_t) { .width = (int16_t) width, .height = (int16_t) (font->ascent + font->descent) };
}
