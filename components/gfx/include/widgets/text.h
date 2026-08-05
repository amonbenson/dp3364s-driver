#pragma once

#include "gfx_elem.h"
#include "gfx_font.h"

typedef struct {
    const char *text;
    const gfx_font_t *font; // required - callers building this directly in C must resolve
                             // a font (e.g. via gfx_font_get()) themselves; the Lua binding
                             // does this from an optional font name, defaulting when omitted
    gfx_appearance_t appearance;
} gfx_text_config_t;

#define GFX_TEXT_CONFIG_DEFAULT (gfx_text_config_t) { \
    .text = "", \
    .font = NULL, \
    .appearance = GFX_APPEARANCE_PRIMARY, \
}

typedef struct {
    gfx_elem_t base;
    gfx_text_config_t config;
} gfx_text_t;

void gfx_text_create(gfx_elem_context_t *ctx, gfx_text_t *elem, const gfx_text_config_t *config);
