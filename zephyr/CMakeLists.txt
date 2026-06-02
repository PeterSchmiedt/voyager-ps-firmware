message(STATUS "[voyager-ps] CMakeLists.txt processed (firmware module)")

zephyr_library_named(voyager_ps_force_on)

# Use *_ifdef which is deferred until Kconfig is fully resolved. A plain
# if(CONFIG_*) in module CMakeLists evaluates too early — the shield's
# .conf hasn't propagated to CMake-scope CONFIG_* vars yet at that point.
# This also avoids linking rgb_force_on.c into builds that don't have
# RGB underglow enabled (e.g. settings_reset shield), where the symbol
# zmk_rgb_underglow_on is undefined.
zephyr_library_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW ${CMAKE_CURRENT_SOURCE_DIR}/rgb_force_on.c)

# ZMK adds its include/ to the 'app' target PRIVATELY, so external module
# libraries don't inherit it. Pull it in explicitly so we can reach
# zmk/rgb_underglow.h.
zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
