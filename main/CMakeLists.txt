idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer dp3364s gfx lua_gfx
)

# Note: you must have a partition named "assets" in your partition table
littlefs_create_partition_image(assets ../assets FLASH_IN_PROJECT)
