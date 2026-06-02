if(CONFIG_ZMK_RGB_UNDERGLOW)
    zephyr_library()
    zephyr_library_sources(rgb_force_on.c)
endif()
