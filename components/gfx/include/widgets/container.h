#pragma once

#include <stdint.h>
#include "gfx_elem.h"

typedef struct {
    gfx_direction_t direction;
    gfx_size_t grow;
} gfx_container_config_t;

#define GFX_CONTAINER_CONFIG_DEFAULT (gfx_container_config_t) { \
    .direction = GFX_DIRECTION_VERTICAL, \
    .grow = { 1, 1 }, \
}

typedef struct {
    gfx_elem_t base;
    gfx_container_config_t config;
} gfx_container_t;

void gfx_container_create(gfx_elem_context_t *ctx, gfx_container_t *elem, const gfx_container_config_t *config);
