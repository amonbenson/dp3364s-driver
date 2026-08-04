idf_component_register(
    SRCS "lua_gfx.c"
    INCLUDE_DIRS "include"
    REQUIRES lua gfx
)

# The lua component builds its library with LUA_32BITS (making lua_Integer/
# lua_Number 32-bit), but only applies that define PRIVATE to itself. Any
# caller of the Lua C API - including main.c, transitively - needs the same
# define or lua_Integer mismatches the ABI liblua.a was actually built with.
target_compile_definitions(${COMPONENT_LIB} PUBLIC LUA_32BITS=1)
