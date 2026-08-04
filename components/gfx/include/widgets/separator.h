#pragma once

#include <stdint.h>
#include "gfx_elem.h"

typedef struct {
    gfx_appearance_t appearance;
    gfx_direction_t direction;
    int16_t grow;
    int16_t length;
} gfx_separator_config_t;

#define GFX_SEPARATOR_CONFIG_DEFAULT (gfx_separator_config_t) { \
    .direction = GFX_DIRECTION_HORIZONTAL, \
    .appearance = GFX_APPEARANCE_PRIMARY, \
    .grow = 1, \
    .length = 1, \
}

typedef struct {
    gfx_elem_t base;
    gfx_separator_config_t config;
} gfx_separator_t;

void gfx_separator_create(gfx_elem_context_t *ctx, gfx_separator_t *elem, const gfx_separator_config_t *config);
