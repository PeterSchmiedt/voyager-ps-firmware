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
        # Make save_state synchronous (bypass the debounced workqueue). Replaces
        # the body of zmk_rgb_underglow_save_state to call settings_save_one
        # directly. Removes any possibility that the user power-cycles before
        # the debounced work runs, or that the work fires with stale state.
        string(REPLACE
            "int zmk_rgb_underglow_save_state(void) {\n#if IS_ENABLED(CONFIG_SETTINGS)\n    int ret = k_work_reschedule(&underglow_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));\n    return MIN(ret, 0);\n#else\n    return 0;\n#endif\n}"
            "int zmk_rgb_underglow_save_state(void) {\n#if IS_ENABLED(CONFIG_SETTINGS)\n    return settings_save_one(\"rgb/underglow/state\", &state, sizeof(state));\n#else\n    return 0;\n#endif\n}"
            _zmk_rgb_patched "${_zmk_rgb_patched}"
        )

        # Also add save_state at the END of zmk_rgb_underglow_init so the boot
        # state gets persisted on FIRST boot too — without this, NVS stays
        # empty until user triggers any save, and the size-check guard in
        # rgb_settings_set could reject any leftover wrong-size data forever.
        string(REPLACE
            "    if (state.on) {\n        k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));\n    }\n\n    return 0;\n}\n\nint zmk_rgb_underglow_save_state"
            "    if (state.on) {\n        k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));\n    }\n\n    zmk_rgb_underglow_save_state();\n    return 0;\n}\n\nint zmk_rgb_underglow_save_state"
            _zmk_rgb_patched "${_zmk_rgb_patched}"
        )
        file(WRITE "${_zmk_rgb}" "${_zmk_rgb_patched}")
        message(STATUS "[voyager-ps] Patched ${_zmk_rgb} — set_hsb persists, init saves boot state")

        # Show both patched functions to verify edits took effect.
        execute_process(
            COMMAND grep -n -A3 -B1 "state\\.color = color" "${_zmk_rgb}"
            OUTPUT_VARIABLE _verify_set_hsb
        )
        execute_process(
            COMMAND grep -n -A3 "int zmk_rgb_underglow_save_state(void)" "${_zmk_rgb}"
            OUTPUT_VARIABLE _verify_save
        )
        message(STATUS "[voyager-ps] Patched set_hsb:\n${_verify_set_hsb}")
        message(STATUS "[voyager-ps] Patched save_state:\n${_verify_save}")
    else()
        message(STATUS "[voyager-ps] ${_zmk_rgb} already patched")
    endif()
else()
    message(FATAL_ERROR "[voyager-ps] Could not find ${_zmk_rgb} to patch!")
endif()

# Also keep the position_state_changed listener as a safety net.
# Source must be added to the main zephyr library so the linker
# pulls in the section-only-referenced subscription symbol.
zephyr_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW rgb_save_on_press.c)
zephyr_include_directories(${CMAKE_SOURCE_DIR}/include)
