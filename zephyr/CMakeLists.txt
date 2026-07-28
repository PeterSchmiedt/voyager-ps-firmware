# Configure-time patch against the west-managed ZMK checkout.
#
# Upstream ZMK does not persist RGB colour or brightness — verified against both
# v0.3.0 and main on 2026-07-28. The chain is:
#
#   &rgb_ug RGB_BRI
#     -> behavior_rgb_underglow.c: binding_convert_central_state_dependent_params
#        rewrites param1 to RGB_COLOR_HSB_CMD (same for RGB_BRD / RGB_HUI /
#        RGB_HUD / RGB_SAI / RGB_SAD)
#     -> on_keymap_binding_pressed: RGB_COLOR_HSB_CMD -> zmk_rgb_underglow_set_hsb()
#     -> set_hsb() assigns state.color and returns 0, never scheduling a save
#
# So every brightness, hue and saturation change is RAM-only and resets to the
# ZMK_RGB_UNDERGLOW_*_START defaults on the next boot. Only RGB_TOG / RGB_ON /
# RGB_OFF (which route to on()/off()) and effect selection persist on their own.
# Patch set_hsb to save too. zmk_rgb_underglow_save_state() is defined earlier in
# the same file and already no-ops when CONFIG_SETTINGS is off, so no guard or
# forward declaration is needed.
#
# The patch is idempotent — a CI cache hit restores an already-patched tree and
# re-running is a no-op. It fails loudly rather than silently skipping: a silent
# no-op would drop lighting persistence again with no signal, which is exactly
# how this regressed before.

set(VPS_RGB_SRC "${APPLICATION_SOURCE_DIR}/src/rgb_underglow.c")

if(NOT EXISTS "${VPS_RGB_SRC}")
    message(FATAL_ERROR
        "voyager_ps: cannot find ${VPS_RGB_SRC} to patch for RGB persistence. "
        "If upstream ZMK moved rgb_underglow.c, update VPS_RGB_SRC in "
        "firmware/zephyr/CMakeLists.txt.")
endif()

file(READ "${VPS_RGB_SRC}" VPS_RGB_CONTENTS)

set(VPS_RGB_NEEDLE  "    state.color = color;\n\n    return 0;\n}")
set(VPS_RGB_PATCHED "    state.color = color;\n\n    return zmk_rgb_underglow_save_state();\n}")

string(FIND "${VPS_RGB_CONTENTS}" "${VPS_RGB_PATCHED}" VPS_RGB_ALREADY)

if(VPS_RGB_ALREADY GREATER_EQUAL 0)
    message(STATUS "voyager_ps: rgb_underglow.c already persists HSB — patch skipped")
else()
    string(FIND "${VPS_RGB_CONTENTS}" "${VPS_RGB_NEEDLE}" VPS_RGB_AT)

    if(VPS_RGB_AT LESS 0)
        message(FATAL_ERROR
            "voyager_ps: zmk_rgb_underglow_set_hsb() no longer matches the shape "
            "this patch expects in ${VPS_RGB_SRC}. Upstream probably refactored "
            "it. Check whether set_hsb now persists on its own — if so delete "
            "this patch, otherwise update VPS_RGB_NEEDLE in "
            "firmware/zephyr/CMakeLists.txt.")
    endif()

    string(REPLACE "${VPS_RGB_NEEDLE}" "${VPS_RGB_PATCHED}"
           VPS_RGB_CONTENTS "${VPS_RGB_CONTENTS}")
    file(WRITE "${VPS_RGB_SRC}" "${VPS_RGB_CONTENTS}")

    message(STATUS "voyager_ps: patched rgb_underglow.c — set_hsb now saves HSB to NVS")
endif()
