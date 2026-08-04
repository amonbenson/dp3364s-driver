#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "dp3364s.h"
#include "gfx_prim.h"
#include "lua_gfx.h"

static const char *TAG = "dp3364s_driver";

#define SCRIPT_PATH "/assets/widget_example.lua"
#define RENDER_INTERVAL_MS 1000 / 50
#define FPS_REPORT_INTERVAL_US 5000000

static void call_lua(lua_State *L, const char *fn) {
    lua_getglobal(L, fn);

    // Silently ignore if the function is not defined, since scripts may not implement all callbacks.
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        ESP_LOGE(TAG, "%s() failed: %s", fn, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(dp3364s_init());

    esp_vfs_littlefs_conf_t fs_conf = {
        .base_path = "/assets",
        .partition_label = "assets",
        .format_if_mount_failed = true,
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&fs_conf));

    static const gfx_prim_context_t ctx = {
        .width = DP3364S_WIDTH,
        .height = DP3364S_HEIGHT,
        .set_pixel = dp3364s_set_pixel,
    };

    lua_State *L = luaL_newstate();

    /* Only the primitives a script needs, not the full stdlib - scripts will
     * eventually arrive from a web interface, so no io/os/package/debug. */
    static const luaL_Reg libs[] = {
        { LUA_GNAME, luaopen_base },
        { LUA_MATHLIBNAME, luaopen_math },
        { LUA_STRLIBNAME, luaopen_string },
        { LUA_TABLIBNAME, luaopen_table },
        { NULL, NULL },
    };
    for (const luaL_Reg *lib = libs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    lua_gfx_open(L, &ctx);

    if (luaL_dofile(L, SCRIPT_PATH) != LUA_OK) {
        ESP_LOGE(TAG, "failed to load %s: %s", SCRIPT_PATH, lua_tostring(L, -1));
        return;
    }

    int64_t last_report = esp_timer_get_time();
    int frames = 0;

    while (1) {
        dp3364s_clear();

        call_lua(L, "render");

        dp3364s_update();
        vTaskDelay(pdMS_TO_TICKS(RENDER_INTERVAL_MS));

        frames++;
        int64_t now = esp_timer_get_time();
        if (now - last_report >= FPS_REPORT_INTERVAL_US) {
            ESP_LOGI(TAG, "%.1f fps", frames * 1e6 / (double) (now - last_report));
            last_report = now;
            frames = 0;
        }
    }
}
