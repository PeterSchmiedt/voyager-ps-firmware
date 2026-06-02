if(CONFIG_ZMK_RGB_UNDERGLOW)
    # Use zephyr_sources instead of zephyr_library() — the latter creates a
    # named library from the current dir, which here is "zephyr" and could
    # collide with Zephyr's own naming. zephyr_sources adds to the main app.
    zephyr_sources(rgb_force_on.c)
endif()
