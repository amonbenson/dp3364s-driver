#pragma once

#include <stdint.h>

/* One glyph's bitmap and metrics, as parsed from a BDF STARTCHAR block. The
 * bitmap is `height` rows of ceil(width/8) bytes each (MSB = leftmost
 * pixel), stored back to back in the font's shared bitmap buffer starting at
 * bitmap_offset - this is exactly how BDF already stores each row, so no
 * repacking is needed while parsing.
 *
 * xoff/yoff place the bitmap relative to the glyph origin (on the baseline,
 * at the pen position), per the BDF BBX field: the bitmap's bottom-left
 * pixel sits at (xoff, yoff) in a y-up coordinate system. dwidth is how far
 * the pen advances before the next glyph. */
typedef struct {
    uint32_t codepoint;
    uint8_t width;
    uint8_t height;
    int8_t xoff;
    int8_t yoff;
    uint8_t dwidth;
    uint16_t bitmap_offset;
} gfx_glyph_t;

typedef struct {
    int8_t ascent;
    int8_t descent;
    gfx_glyph_t *glyphs; // sorted ascending by codepoint
    uint16_t glyph_count;
    uint8_t *bitmap;
} gfx_font_t;

/* The font gfx_font_get() resolves a NULL/omitted name to. */
#define GFX_FONT_DEFAULT_NAME "tb-8"

/* Parses a BDF font from `path` (e.g. a LittleFS path such as
 * "/assets/fonts/tb-8.bdf"). Returns NULL and logs the reason on any I/O or
 * format error. The caller owns the returned font and must free it with
 * gfx_font_free() - most callers want gfx_font_get() instead, which caches
 * fonts for the life of the process. */
gfx_font_t *gfx_font_load(const char *path);
void gfx_font_free(gfx_font_t *font);

/* Looks up `name` (NULL for GFX_FONT_DEFAULT_NAME) in a process-wide cache,
 * loading it from "/assets/fonts/<name>.bdf" on first use. Returns the same
 * pointer on every later call with the same name. Cached fonts are never
 * freed - the set of fonts a script uses is small and expected to live for
 * the process lifetime. Returns NULL if the font failed to load. */
const gfx_font_t *gfx_font_get(const char *name);

/* Finds the glyph for `codepoint` in `font`, or NULL if the font has no
 * glyph for it. */
const gfx_glyph_t *gfx_font_find_glyph(const gfx_font_t *font, uint32_t codepoint);

/* Decodes one UTF-8 codepoint from *s and advances *s past it. Malformed
 * sequences decode as U+FFFD and advance by one byte. Must not be called
 * with **s == '\0' - callers loop `while (*s) { ... gfx_utf8_next(&s); }`. */
uint32_t gfx_utf8_next(const char **s);
