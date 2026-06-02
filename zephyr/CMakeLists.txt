# Patch ZMK's rgb_underglow.c to make set_hsb persist via save_state.
#
# ZMK v0.3 rgb_underglow.c:377 set_hsb() is:
#     int zmk_rgb_underglow_set_hsb(struct zmk_led_hsb color) {
#         if (color.h > HUE_MAX || color.s > SAT_MAX || color.b > BRT_MAX) {
#             return -ENOTSUP;
#         }
#         state.color = color;
#         return 0;
#     }
# Every other state-modifying function in the file returns
# zmk_rgb_underglow_save_state() instead of 0. set_hsb is the only one
# that doesn't, and that's why &rgb_ug RGB_COLOR_HSB(...) never persists.
#
# In-place edit at CMake configure time. The ZMK source is in
# ${ZEPHYR_BASE}/../zmk/ on CI (it's a temp west checkout) and in
# firmware/.zmk/zmk/ locally. Either way, we have write access.
set(_zmk_rgb "${ZEPHYR_BASE}/../zmk/app/src/rgb_underglow.c")
if(EXISTS "${_zmk_rgb}")
    file(READ "${_zmk_rgb}" _zmk_rgb_src)
    string(FIND "${_zmk_rgb_src}" "state.color = color;\n\n    return zmk_rgb_underglow_save_state();" _already_patched)
    if(_already_patched EQUAL -1)
        string(REPLACE
            "state.color = color;\n\n    return 0;"
            "state.color = color;\n\n    return zmk_rgb_underglow_save_state();"
            _zmk_rgb_patched "${_zmk_rgb_src}"
        )
        file(WRITE "${_zmk_rgb}" "${_zmk_rgb_patched}")
        message(STATUS "[voyager-ps] Patched ${_zmk_rgb} — set_hsb now persists via save_state")
    else()
        message(STATUS "[voyager-ps] ${_zmk_rgb} already patched")
    endif()
else()
    message(WARNING "[voyager-ps] Could not find ${_zmk_rgb} to patch — RGB persistence will not work!")
endif()

# Also keep the position_state_changed listener as a safety net.
# Source must be added to the main zephyr library so the linker
# pulls in the section-only-referenced subscription symbol.
zephyr_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW rgb_save_on_press.c)
zephyr_include_directories(${CMAKE_SOURCE_DIR}/include)
