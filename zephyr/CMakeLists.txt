message(STATUS "[voyager-ps] CMakeLists.txt processed (firmware module)")

if(CONFIG_ZMK_RGB_UNDERGLOW)
    message(STATUS "[voyager-ps] adding rgb_force_on.c (CONFIG_ZMK_RGB_UNDERGLOW=y)")
    zephyr_library_named(voyager_ps_force_on)
    zephyr_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/rgb_force_on.c)
else()
    message(STATUS "[voyager-ps] skipping rgb_force_on.c (RGB underglow not enabled)")
endif()
