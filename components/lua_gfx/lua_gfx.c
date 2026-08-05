#include "lua_gfx.h"

#include "lauxlib.h"

#include "widgets/container.h"
#include "widgets/separator.h"

// Shared by every widget userdata (gfx.container()/gfx.separator(), including the default
// root): __index carries the tree methods (add_child/remove), and __gc unlinks a widget
// from its parent when Lua collects it (e.g. once nothing references it after being
// replaced as root or removed from its parent).
#define ELEM_METATABLE "gfx.elem"

// Registry key (its address is the key) for a table that keeps a strong Lua reference to
// every element currently linked into the tree. The tree's parent/children/next_sibling
// pointers are invisible to the Lua GC, so without this an element attached via
// elem:add_child() but not held by any live Lua variable could be collected while still
// referenced from C.
static int keepalive_registry_key;

// Registry key holding whatever element root() last set as the tree root (seeded with a
// default container by lua_gfx_open() - see there). Stored directly under the registry
// rather than as a plain Lua global so a script can't shadow/clobber it by accident, and
// so lua_gfx_get_root() has a single well-known slot to read back from C each frame.
// Being a registry value is itself enough to keep it alive for the GC, same as the
// keepalive table above.
static int root_registry_key;

static gfx_elem_context_t *ctx_of(lua_State *L) {
    return (gfx_elem_context_t *) lua_touserdata(L, lua_upvalueindex(1));
}

// Valid for both light userdata (ROOT) and full userdata (owned widgets), since every
// widget struct starts with a gfx_elem_t base, so the userdata block's address already
// *is* a valid gfx_elem_t*.
static gfx_elem_t *check_elem(lua_State *L, int idx) {
    luaL_argcheck(L, lua_isuserdata(L, idx), idx, "expected a gfx element");
    return (gfx_elem_t *) lua_touserdata(L, idx);
}

static void keepalive_set(lua_State *L, int elem_idx, bool alive) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, &keepalive_registry_key);
    lua_pushvalue(L, elem_idx);
    if (alive) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static int l_elem_add_child(lua_State *L) {
    gfx_elem_t *parent = check_elem(L, 1);
    gfx_elem_t *child = check_elem(L, 2);

    gfx_elem_add_child(parent, child);
    keepalive_set(L, 2, true);

    return 0;
}

static int l_elem_remove(lua_State *L) {
    gfx_elem_t *elem = check_elem(L, 1);

    gfx_elem_remove(elem);
    keepalive_set(L, 1, false);

    return 0;
}

// root(elem): sets elem as the tree root main.c updates/renders every frame, and returns
// it back (so it can double as e.g. `local box = root(gfx.container({...}))`).
static int l_root(lua_State *L) {
    check_elem(L, 1);

    lua_pushvalue(L, 1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &root_registry_key);

    lua_pushvalue(L, 1);
    return 1;
}

static int l_elem_gc(lua_State *L) {
    // Only ever called for owned (full userdata) widgets - light userdata such as ROOT
    // has no separate allocation for Lua to collect.
    gfx_elem_remove((gfx_elem_t *) lua_touserdata(L, 1));
    return 0;
}

static void opt_int_field(lua_State *L, int idx, const char *key, lua_Integer *out) {
    lua_getfield(L, idx, key);
    if (!lua_isnil(L, -1)) {
        *out = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
}

static void opt_size_field(lua_State *L, int idx, const char *key, gfx_size_t *out) {
    lua_getfield(L, idx, key);
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);

        lua_geti(L, -1, 1);
        out->width = (int16_t) luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        lua_geti(L, -1, 2);
        out->height = (int16_t) luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static int l_container(lua_State *L) {
    gfx_elem_context_t *ctx = ctx_of(L);
    gfx_container_config_t config = GFX_CONTAINER_CONFIG_DEFAULT;

    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);

        lua_Integer direction = config.direction;
        opt_int_field(L, 1, "direction", &direction);
        config.direction = (gfx_direction_t) direction;

        opt_size_field(L, 1, "grow", &config.grow);
    }

    gfx_container_t *elem = (gfx_container_t *) lua_newuserdata(L, sizeof(gfx_container_t));
    gfx_container_create(ctx, elem, &config);

    luaL_setmetatable(L, ELEM_METATABLE);
    return 1;
}

static int l_separator(lua_State *L) {
    gfx_elem_context_t *ctx = ctx_of(L);
    gfx_separator_config_t config = GFX_SEPARATOR_CONFIG_DEFAULT;

    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);

        lua_Integer direction = config.direction;
        opt_int_field(L, 1, "direction", &direction);
        config.direction = (gfx_direction_t) direction;

        lua_Integer appearance = config.appearance;
        opt_int_field(L, 1, "appearance", &appearance);
        config.appearance = (gfx_appearance_t) appearance;

        lua_Integer grow = config.grow;
        opt_int_field(L, 1, "grow", &grow);
        config.grow = (int16_t) grow;

        lua_Integer length = config.length;
        opt_int_field(L, 1, "length", &length);
        config.length = (int16_t) length;
    }

    gfx_separator_t *elem = (gfx_separator_t *) lua_newuserdata(L, sizeof(gfx_separator_t));
    gfx_separator_create(ctx, elem, &config);

    luaL_setmetatable(L, ELEM_METATABLE);
    return 1;
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

    gfx_draw_line(ctx_of(L)->prim_ctx, p0, p1, color);
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
    { "container", l_container },
    { "separator", l_separator },
    { NULL, NULL },
};

static const luaL_Reg elem_methods[] = {
    { "add_child", l_elem_add_child },
    { "remove", l_elem_remove },
    { NULL, NULL },
};

gfx_elem_t *lua_gfx_get_root(lua_State *L) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, &root_registry_key);
    gfx_elem_t *root = lua_isuserdata(L, -1) ? (gfx_elem_t *) lua_touserdata(L, -1) : NULL;
    lua_pop(L, 1);
    return root;
}

void lua_gfx_open(lua_State *L, gfx_elem_context_t *ctx) {
    lua_pushinteger(L, ctx->prim_ctx->size.width);
    lua_setglobal(L, "WIDTH");
    lua_pushinteger(L, ctx->prim_ctx->size.height);
    lua_setglobal(L, "HEIGHT");

    lua_pushinteger(L, GFX_DIRECTION_HORIZONTAL);
    lua_setglobal(L, "HORIZONTAL");
    lua_pushinteger(L, GFX_DIRECTION_VERTICAL);
    lua_setglobal(L, "VERTICAL");
    lua_pushinteger(L, GFX_APPEARANCE_PRIMARY);
    lua_setglobal(L, "PRIMARY");
    lua_pushinteger(L, GFX_APPEARANCE_SECONDARY);
    lua_setglobal(L, "SECONDARY");
    lua_pushinteger(L, GFX_APPEARANCE_ACCENT);
    lua_setglobal(L, "ACCENT");

    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &keepalive_registry_key);

    luaL_newmetatable(L, ELEM_METATABLE);
    lua_newtable(L);
    luaL_setfuncs(L, elem_methods, 0);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_elem_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // Seed the root registry slot with a default container so main.c always has
    // something to render even if a script never calls root() itself.
    gfx_container_t *default_root = (gfx_container_t *) lua_newuserdata(L, sizeof(gfx_container_t));
    gfx_container_config_t default_root_config = GFX_CONTAINER_CONFIG_DEFAULT;
    gfx_container_create(ctx, default_root, &default_root_config);
    luaL_setmetatable(L, ELEM_METATABLE);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &root_registry_key);

    lua_pushcfunction(L, l_root);
    lua_setglobal(L, "root");

    lua_newtable(L);
    lua_pushlightuserdata(L, (void *) ctx);
    luaL_setfuncs(L, gfx_funcs, 1);
    lua_setglobal(L, "gfx");
}
