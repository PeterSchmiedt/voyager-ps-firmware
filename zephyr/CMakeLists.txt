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

        # REMOVED: previously also patched init to call save_state at the end.
        # That was a bug — init runs BEFORE main's settings_load(), so it was
        # overwriting NVS with defaults on every boot, clobbering the user's
        # saved colour. Boot order is:
        #   1. SYS_INIT (APPLICATION pri): init() runs, state = defaults (10%)
        #   2. init save fires → settings_save_one writes 10% to NVS
        #   3. main(): settings_load() reads NVS → gets the 10% we just wrote
        # So saved values from previous sessions get blown away on every boot.
        # If size-mismatched stale NVS is a concern, fix it with settings_reset.uf2,
        # not by force-overwriting on every init.
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

# Listener removed: with the in-source patches above, set_hsb itself
# triggers save_state synchronously. A separate listener was firing on
# every position_state_changed event — including any phantom matrix
# event during the early boot window (between rgb_underglow_init's
# state=defaults assignment at SYS_INIT and main()'s settings_load).
# A boot-window phantom event would synchronously write defaults to
# NVS, clobbering the saved value just before the load tried to read it.
# Removing the listener eliminates that race.
