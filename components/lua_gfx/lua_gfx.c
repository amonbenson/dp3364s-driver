#include "lua_gfx.h"

#include "lauxlib.h"

static const gfx_elem_context_t *ctx_of(lua_State *L) {
    return lua_touserdata(L, lua_upvalueindex(1));
}

static int l_draw_point(lua_State *L) {
    gfx_point_t p = { (int16_t) luaL_checkinteger(L, 1), (int16_t) luaL_checkinteger(L, 2) };
    gfx_color_t c = {
        (uint8_t) luaL_checkinteger(L, 3),
        (uint8_t) luaL_checkinteger(L, 4),
        (uint8_t) luaL_checkinteger(L, 5),
    };

    gfx_draw_point(ctx_of(L)->prim_ctx, p, c);
    return 0;
}

static int l_draw_line(lua_State *L) {
    gfx_point_t p0 = { (int16_t) luaL_checkinteger(L, 1), (int16_t) luaL_checkinteger(L, 2) };
    gfx_point_t p1 = { (int16_t) luaL_checkinteger(L, 3), (int16_t) luaL_checkinteger(L, 4) };
    gfx_color_t color = {
        (uint8_t) luaL_checkinteger(L, 5),
        (uint8_t) luaL_checkinteger(L, 6),
        (uint8_t) luaL_checkinteger(L, 7),
    };

    gfx_draw_line(ctx_of(L)->prim_ctx, p0, color, p1, color);
    return 0;
}

static int l_draw_rect(lua_State *L) {
    gfx_rect_t rect = {
        .x = (int16_t) luaL_checkinteger(L, 1),
        .y = (int16_t) luaL_checkinteger(L, 2),
        .width = (uint16_t) luaL_checkinteger(L, 3),
        .height = (uint16_t) luaL_checkinteger(L, 4),
    };
    gfx_color_t color = {
        (uint8_t) luaL_checkinteger(L, 5),
        (uint8_t) luaL_checkinteger(L, 6),
        (uint8_t) luaL_checkinteger(L, 7),
    };

    gfx_draw_rect(ctx_of(L)->prim_ctx, rect, color);
    return 0;
}

static const luaL_Reg gfx_funcs[] = {
    { "draw_point", l_draw_point },
    { "draw_line", l_draw_line },
    { "draw_rect", l_draw_rect },
    { NULL, NULL },
};

void lua_gfx_open(lua_State *L, const gfx_elem_context_t *ctx) {
    lua_pushinteger(L, ctx->prim_ctx->size.width);
    lua_setglobal(L, "WIDTH");
    lua_pushinteger(L, ctx->prim_ctx->size.height);
    lua_setglobal(L, "HEIGHT");

    lua_newtable(L);
    lua_pushlightuserdata(L, (void *) ctx);
    luaL_setfuncs(L, gfx_funcs, 1);
    lua_setglobal(L, "gfx");
}
