#include "gfx_font.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "gfx_font";

#define FONT_DIR "/assets/fonts/"
#define FONT_CACHE_MAX 8
#define FONT_NAME_MAX 32
#define LINE_BUF_SIZE 256

static struct {
    char name[FONT_NAME_MAX];
    gfx_font_t *font;
} s_font_cache[FONT_CACHE_MAX];
static int s_font_cache_count = 0;

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static int glyph_cmp(const void *a, const void *b) {
    uint32_t ca = ((const gfx_glyph_t *) a)->codepoint;
    uint32_t cb = ((const gfx_glyph_t *) b)->codepoint;
    return (ca > cb) - (ca < cb);
}

uint32_t gfx_utf8_next(const char **s) {
    const unsigned char *p = (const unsigned char *) *s;

    int extra;
    uint32_t cp;
    if ((p[0] & 0x80) == 0x00) { cp = p[0]; extra = 0; }
    else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1F; extra = 1; }
    else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0F; extra = 2; }
    else if ((p[0] & 0xF8) == 0xF0) { cp = p[0] & 0x07; extra = 3; }
    else {
        *s = (const char *) (p + 1);
        return 0xFFFD;
    }

    for (int i = 1; i <= extra; i++) {
        if (p[i] == '\0' || (p[i] & 0xC0) != 0x80) {
            *s = (const char *) (p + 1);
            return 0xFFFD;
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }

    *s = (const char *) (p + 1 + extra);
    return cp;
}

const gfx_glyph_t *gfx_font_find_glyph(const gfx_font_t *font, uint32_t codepoint) {
    int lo = 0, hi = (int) font->glyph_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (font->glyphs[mid].codepoint == codepoint) {
            return &font->glyphs[mid];
        } else if (font->glyphs[mid].codepoint < codepoint) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

void gfx_font_free(gfx_font_t *font) {
    if (!font) {
        return;
    }
    free(font->glyphs);
    free(font->bitmap);
    free(font);
}

/* BDF is a line-oriented text format. This walks it with a tiny state
 * machine: outside a STARTCHAR..ENDCHAR block we only care about a handful
 * of header keywords, and inside one we track the glyph's own metadata
 * (ENCODING/DWIDTH/BBX) until its BITMAP keyword, after which the following
 * `height` lines are hex-encoded rows consumed directly into font->bitmap -
 * BDF already byte-aligns each row exactly the way we want to store it, so
 * no repacking is needed. */
gfx_font_t *gfx_font_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "failed to open '%s'", path);
        return NULL;
    }

    gfx_font_t *font = calloc(1, sizeof(gfx_font_t));
    if (!font) {
        ESP_LOGE(TAG, "out of memory loading '%s'", path);
        fclose(f);
        return NULL;
    }

    uint16_t glyph_capacity = 0;
    uint32_t bitmap_capacity = 0;
    uint32_t bitmap_size = 0;
    bool have_ascent = false, have_descent = false, have_cap_height = false;
    int bbox_height = 0, bbox_yoff = 0;

    bool in_char = false;   // between STARTCHAR and ENDCHAR
    bool char_valid = false; // current glyph has a usable codepoint and room in the array
    long encoding = -1;
    int dwidth = 0;
    int bbx_w = 0, bbx_h = 0, bbx_xoff = 0, bbx_yoff = 0;
    int bitmap_rows_left = 0;
    bool overflow = false;
    bool oom = false;

    char line[LINE_BUF_SIZE];
    while (fgets(line, sizeof(line), f)) {
        if (bitmap_rows_left > 0) {
            int row_bytes = (bbx_w + 7) / 8;
            if (char_valid) {
                if (bitmap_size + row_bytes > UINT16_MAX) {
                    overflow = true;
                    char_valid = false;
                } else {
                    if (bitmap_size + row_bytes > bitmap_capacity) {
                        uint32_t new_capacity = bitmap_capacity ? bitmap_capacity * 2 : 512;
                        if (new_capacity < bitmap_size + row_bytes) {
                            new_capacity = bitmap_size + row_bytes;
                        }
                        uint8_t *grown = realloc(font->bitmap, new_capacity);
                        if (!grown) {
                            oom = true;
                            break;
                        }
                        font->bitmap = grown;
                        bitmap_capacity = new_capacity;
                    }
                    for (int i = 0; i < row_bytes; i++) {
                        font->bitmap[bitmap_size + i] = (uint8_t) ((hex_val(line[i * 2]) << 4) | hex_val(line[i * 2 + 1]));
                    }
                    bitmap_size += row_bytes;
                }
            }
            bitmap_rows_left--;
            continue;
        }

        if (!in_char) {
            if (strncmp(line, "FONT_ASCENT ", 12) == 0) {
                font->ascent = (int8_t) atoi(line + 12);
                have_ascent = true;
            } else if (strncmp(line, "FONT_DESCENT ", 13) == 0) {
                font->descent = (int8_t) atoi(line + 13);
                have_descent = true;
            } else if (strncmp(line, "CAP_HEIGHT ", 11) == 0) {
                font->cap_height = (int8_t) atoi(line + 11);
                have_cap_height = true;
            } else if (strncmp(line, "FONTBOUNDINGBOX ", 17) == 0) {
                int w, h, xo, yo;
                if (sscanf(line + 17, "%d %d %d %d", &w, &h, &xo, &yo) == 4) {
                    bbox_height = h;
                    bbox_yoff = yo;
                }
            } else if (strncmp(line, "CHARS ", 6) == 0) {
                glyph_capacity = (uint16_t) atoi(line + 6);
                if (glyph_capacity > 0) {
                    font->glyphs = malloc(glyph_capacity * sizeof(gfx_glyph_t));
                    if (!font->glyphs) {
                        oom = true;
                        break;
                    }
                }
            } else if (strncmp(line, "STARTCHAR", 9) == 0) {
                if (!have_ascent || !have_descent) {
                    // No explicit FONT_ASCENT/FONT_DESCENT properties - fall back
                    // to the font's overall bounding box.
                    font->ascent = (int8_t) (bbox_height + bbox_yoff);
                    font->descent = (int8_t) (-bbox_yoff);
                    have_ascent = have_descent = true;
                }
                if (!have_cap_height) {
                    font->cap_height = font->ascent;
                }
                in_char = true;
                char_valid = false;
                encoding = -1;
            }
            continue;
        }

        if (strncmp(line, "ENCODING ", 9) == 0) {
            encoding = atol(line + 9);
        } else if (strncmp(line, "DWIDTH ", 7) == 0) {
            dwidth = atoi(line + 7);
        } else if (strncmp(line, "BBX ", 4) == 0) {
            sscanf(line + 4, "%d %d %d %d", &bbx_w, &bbx_h, &bbx_xoff, &bbx_yoff);
        } else if (strncmp(line, "BITMAP", 6) == 0) {
            // Glyphs with no standard Unicode mapping (ENCODING -1) are skipped -
            // they can never be looked up by codepoint - as is any glyph beyond
            // what CHARS promised the array room for.
            char_valid = encoding >= 0 && font->glyph_count < glyph_capacity;
            if (char_valid) {
                gfx_glyph_t *glyph = &font->glyphs[font->glyph_count++];
                glyph->codepoint = (uint32_t) encoding;
                glyph->width = (uint8_t) bbx_w;
                glyph->height = (uint8_t) bbx_h;
                glyph->xoff = (int8_t) bbx_xoff;
                glyph->yoff = (int8_t) bbx_yoff;
                glyph->dwidth = (uint8_t) dwidth;
                glyph->bitmap_offset = (uint16_t) bitmap_size;
            }
            bitmap_rows_left = bbx_h;
        } else if (strncmp(line, "ENDCHAR", 7) == 0) {
            in_char = false;
        }
    }

    fclose(f);

    if (oom) {
        ESP_LOGE(TAG, "out of memory loading '%s'", path);
        gfx_font_free(font);
        return NULL;
    }

    if (overflow) {
        ESP_LOGW(TAG, "'%s' exceeds the %u byte bitmap limit, some glyphs were dropped", path, (unsigned) UINT16_MAX);
    }

    if (font->glyph_count == 0) {
        ESP_LOGE(TAG, "'%s' has no usable glyphs", path);
        gfx_font_free(font);
        return NULL;
    }

    qsort(font->glyphs, font->glyph_count, sizeof(gfx_glyph_t), glyph_cmp);

    ESP_LOGI(TAG, "loaded '%s': %u glyphs, %u bytes of bitmap data", path, font->glyph_count, (unsigned) bitmap_size);
    return font;
}

const gfx_font_t *gfx_font_get(const char *name) {
    if (!name) {
        name = GFX_FONT_DEFAULT_NAME;
    }

    for (int i = 0; i < s_font_cache_count; i++) {
        if (strcmp(s_font_cache[i].name, name) == 0) {
            return s_font_cache[i].font;
        }
    }

    if (s_font_cache_count >= FONT_CACHE_MAX) {
        ESP_LOGE(TAG, "font cache full (max %d), cannot load '%s'", FONT_CACHE_MAX, name);
        return NULL;
    }

    char path[sizeof(FONT_DIR) + FONT_NAME_MAX + 4];
    snprintf(path, sizeof(path), FONT_DIR "%s.bdf", name);

    gfx_font_t *font = gfx_font_load(path);
    if (!font) {
        return NULL;
    }

    strncpy(s_font_cache[s_font_cache_count].name, name, FONT_NAME_MAX - 1);
    s_font_cache[s_font_cache_count].font = font;
    s_font_cache_count++;
    return font;
}
