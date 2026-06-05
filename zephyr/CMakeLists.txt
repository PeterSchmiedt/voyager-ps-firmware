# RGB-underglow persistence fix + minimal NVS diagnostic logging.
#
# Configure-time patches against stock ZMK rgb_underglow.c:
#
#   1. set_hsb persistence fix — change the final `return 0;` to
#      `return zmk_rgb_underglow_save_state();`.
#      Stock ZMK rewrites every encoder RGB_BRI/RGB_BRD binding into
#      RGB_COLOR_HSB_CMD via binding_convert_central_state_dependent_params,
#      then dispatches RGB_COLOR_HSB_CMD to set_hsb. Stock set_hsb
#      updates state.color in RAM and returns 0 — never calls
#      save_state — so encoder-driven brightness changes were lost on
#      reboot. This is the actual functional fix.
#
#   2. Boot-time + NVS-write LOG_INF instrumentation only:
#      - settings_set entry      : settings subsystem invoked the callback
#      - settings_set loaded     : what bytes NVS gave back
#      - init complete           : final state after defaults+settings_load
#      - save_work fired/done    : every debounced NVS write
#
# Hot-path logging (set_hsb / calc_brt / behavior convert+pressed+released
# / change_brt / save_state schedule) has been REMOVED. It was lengthening
# the system workqueue's per-event processing time at high encoder rotation
# rates, which in turn lengthened the window in which ZMK's EC11 driver
# leaves both A and B GPIO interrupts disabled (see
# app/module/drivers/sensor/ec11/ec11_trigger.c — ec11_a_gpio_callback
# disables interrupts via setup_int(false), submits work, and only
# re-enables them in ec11_thread_cb AFTER the handler runs). Fast twists
# happening during that window have their quadrature edges silently
# dropped, manifesting as the "stuck" symptom.
#
# CAUTION: the ZMK CI build-user-config.yml workflow caches
# ${base_dir}/zmk/ keyed on hashFiles('**/west.yml', '**/build.yaml').
# When those don't change, cached ZMK source from a prior build is
# restored with prior patches baked in, and apply_patch's anchor (stock
# function body) won't match. To force a fresh checkout when this file
# changes, touch build.yaml (a comment edit is enough — hashFiles hashes
# content, not metadata).

function(apply_patch label file anchor replacement)
    if(NOT EXISTS "${file}")
        message(WARNING "[VPS] ${label}: file not found: ${file}")
        return()
    endif()
    file(READ "${file}" _content)
    string(FIND "${_content}" "${replacement}" _already)
    if(_already GREATER -1)
        message(STATUS "[VPS] ${label}: already applied")
        return()
    endif()
    string(FIND "${_content}" "${anchor}" _anchor_at)
    if(_anchor_at EQUAL -1)
        message(WARNING "[VPS] ${label}: anchor not found in ${file}")
        return()
    endif()
    string(REPLACE "${anchor}" "${replacement}" _patched "${_content}")
    file(WRITE "${file}" "${_patched}")
    message(STATUS "[VPS] ${label}: applied")
endfunction()

set(_zmk_rgb "${ZEPHYR_BASE}/../zmk/app/src/rgb_underglow.c")

# --- THE FIX: set_hsb now persists state via save_state.
# Anchor uses the `state.color = color;` + `return 0;` end-of-function
# pattern, which is unique to set_hsb among rgb_underglow.c's functions
# (siblings use `state.color = zmk_rgb_underglow_calc_*()`).
apply_patch("set_hsb persistence fix"
    "${_zmk_rgb}"
[==[    state.color = color;

    return 0;
}
]==]
[==[    state.color = color;

    return zmk_rgb_underglow_save_state();
}
]==])

# --- Boot-time NVS load: did the settings_set callback fire, and what
# did it read for "rgb/underglow/state"?
apply_patch("settings_set entry"
    "${_zmk_rgb}"
[==[static int rgb_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;

    if (settings_name_steq(name, "state", &next) && !next) {
]==]
[==[static int rgb_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;

    LOG_INF("[VPS] settings_set entry: name=\"%s\" len=%u sizeof(state)=%u",
            name, (unsigned)len, (unsigned)sizeof(state));

    if (settings_name_steq(name, "state", &next) && !next) {
]==])

apply_patch("settings_set loaded"
    "${_zmk_rgb}"
[==[        rc = read_cb(cb_arg, &state, sizeof(state));
        if (rc >= 0) {
            if (state.on) {
                k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));
            }
]==]
[==[        rc = read_cb(cb_arg, &state, sizeof(state));
        LOG_INF("[VPS] settings_set loaded: rc=%d state {on=%d h=%u s=%u b=%u eff=%u spd=%u}",
                rc, state.on, state.color.h, state.color.s, state.color.b,
                state.current_effect, state.animation_speed);
        if (rc >= 0) {
            if (state.on) {
                k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));
            }
]==])

# --- Every NVS write: what gets written and what rc came back.
apply_patch("save_work fired"
    "${_zmk_rgb}"
[==[static void zmk_rgb_underglow_save_state_work(struct k_work *_work) {
    settings_save_one("rgb/underglow/state", &state, sizeof(state));
}
]==]
[==[static void zmk_rgb_underglow_save_state_work(struct k_work *_work) {
    LOG_INF("[VPS] save_work fired: writing state {on=%d h=%u s=%u b=%u eff=%u spd=%u}",
            state.on, state.color.h, state.color.s, state.color.b,
            state.current_effect, state.animation_speed);
    int _vps_rc = settings_save_one("rgb/underglow/state", &state, sizeof(state));
    LOG_INF("[VPS] save_work done: settings_save_one rc=%d", _vps_rc);
}
]==])

# --- End of init: final state after Kconfig defaults + settings_load.
apply_patch("init complete"
    "${_zmk_rgb}"
[==[#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_AUTO_OFF_USB)
    state.on = zmk_usb_is_powered();
#endif

    if (state.on) {
        k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));
    }

    return 0;
}
]==]
[==[#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_AUTO_OFF_USB)
    state.on = zmk_usb_is_powered();
#endif

    LOG_INF("[VPS] init complete: state {on=%d h=%u s=%u b=%u eff=%u spd=%u}",
            state.on, state.color.h, state.color.s, state.color.b,
            state.current_effect, state.animation_speed);

    if (state.on) {
        k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));
    }

    return 0;
}
]==])
