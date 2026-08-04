#pragma once

#include "lua.h"

#include "gfx_elem.h"

/* Registers a "gfx" global in L: gfx.point/gfx.line bound to ctx, plus
 * gfx.WIDTH/gfx.HEIGHT. HTTP fetch and gfx_elem bindings will join this
 * module later; keep new bindings here rather than spreading them out. */
void lua_gfx_open(lua_State *L, const gfx_elem_context_t *ctx);
