#pragma once

#include "lua.h"

#include "gfx_elem.h"

/* Registers globals in L:
 *  - WIDTH/HEIGHT, and a "gfx" table with immediate-mode gfx.draw_point/
 *    draw_line/draw_rect bound to ctx.
 *  - gfx.container(config)/gfx.separator(config), which create widgets bound
 *    to ctx and linked into the tree once attached via elem:add_child().
 *  - root(elem): sets elem as the tree root main.c updates/renders every
 *    frame, and returns it back. Seeded with a default container internally
 *    (not exposed to Lua) so lua_gfx_get_root() has something to return even
 *    if a script never calls root() itself.
 *  - direction/appearance constants (HORIZONTAL, VERTICAL, PRIMARY,
 *    SECONDARY, ACCENT) for use in widget config tables.
 * HTTP fetch bindings will join this module later; keep new bindings here
 * rather than spreading them out. */
void lua_gfx_open(lua_State *L, gfx_elem_context_t *ctx);

/* Reads back the element root() last set, or the internal default if a
 * script never called it. Returns NULL only if that slot somehow ended up
 * holding a non-element value; callers should skip that frame's
 * update/render rather than treat it as fatal, since it reflects untrusted
 * script state. */
gfx_elem_t *lua_gfx_get_root(lua_State *L);
