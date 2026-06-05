# RGB-underglow persistence fix + debug instrumentation.
#
# At CMake configure time, edits ZMK's stock rgb_underglow.c and
# behavior_rgb_underglow.c to:
#   1. Make zmk_rgb_underglow_set_hsb call save_state — this is the
#      actual fix for the persistence bug. Stock ZMK rewrites every
#      RGB_BRI/RGB_BRD keymap binding to RGB_COLOR_HSB_CMD via
#      binding_convert_central_state_dependent_params, which dispatches
#      to set_hsb. Stock set_hsb returns 0 without persisting, so all
#      encoder-driven brightness changes are lost on reboot. (Confirmed
#      by USB CDC trace before this patch was applied.)
#   2. Inject [VPS]-tagged LOG_INF statements at every relevant decision
#      point so the encoder → behavior → underglow → NVS path stays
#      visible in future captures.
#
# Each apply_log_patch() call is idempotent: it bails if the marker text
# is already present. The ZMK source path differs between local
# (firmware/.zmk/zmk) and CI (a west-managed tmp dir), but in both cases
# ${ZEPHYR_BASE}/../zmk/app/src/... is the right place.
#
# To remove: comment out the apply_log_patch() calls below and either
# delete .zmk and re-run `west update`, or wait for the next CI build
# (which always starts from a fresh checkout).

function(apply_log_patch label file anchor replacement)
    if(NOT EXISTS "${file}")
        message(WARNING "[VPS][LOGGING] ${label}: file not found: ${file}")
        return()
    endif()
    file(READ "${file}" _content)
    string(FIND "${_content}" "${replacement}" _already)
    if(_already GREATER -1)
        message(STATUS "[VPS][LOGGING] ${label}: already applied")
        return()
    endif()
    string(FIND "${_content}" "${anchor}" _anchor_at)
    if(_anchor_at EQUAL -1)
        message(WARNING "[VPS][LOGGING] ${label}: anchor not found in ${file}")
        return()
    endif()
    string(REPLACE "${anchor}" "${replacement}" _patched "${_content}")
    file(WRITE "${file}" "${_patched}")
    message(STATUS "[VPS][LOGGING] ${label}: applied")
endfunction()

set(_zmk_rgb "${ZEPHYR_BASE}/../zmk/app/src/rgb_underglow.c")
set(_zmk_beh "${ZEPHYR_BASE}/../zmk/app/src/behaviors/behavior_rgb_underglow.c")

# =====================================================================
# rgb_underglow.c
# =====================================================================

# --- settings_set: entry (called by settings subsystem during settings_load)
apply_log_patch("settings_set entry"
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

# --- settings_set: after NVS read, log the loaded values
apply_log_patch("settings_set loaded"
    "${_zmk_rgb}"
[==[        rc = read_cb(cb_arg, &state, sizeof(state));
        if (rc >= 0) {
            if (state.on) {
                k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));
            }
]==]
[==[        rc = read_cb(cb_arg, &state, sizeof(state));
        LOG_INF("[VPS] settings_set loaded: rc=%d state {on=%d h=%u s=%u b=%u eff=%u spd=%u step=%u}",
                rc, state.on, state.color.h, state.color.s, state.color.b,
                state.current_effect, state.animation_speed, state.animation_step);
        if (rc >= 0) {
            if (state.on) {
                k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(50));
            }
]==])

# --- save_state_work: the debounced flush to NVS
apply_log_patch("save_work fired"
    "${_zmk_rgb}"
[==[static void zmk_rgb_underglow_save_state_work(struct k_work *_work) {
    settings_save_one("rgb/underglow/state", &state, sizeof(state));
}
]==]
[==[static void zmk_rgb_underglow_save_state_work(struct k_work *_work) {
    LOG_INF("[VPS] save_work fired: writing state {on=%d h=%u s=%u b=%u eff=%u spd=%u} to NVS",
            state.on, state.color.h, state.color.s, state.color.b,
            state.current_effect, state.animation_speed);
    int _vps_rc = settings_save_one("rgb/underglow/state", &state, sizeof(state));
    LOG_INF("[VPS] save_work done: settings_save_one rc=%d", _vps_rc);
}
]==])

# --- init: log final state after defaults + settings_load have both run
apply_log_patch("init complete"
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

# --- save_state: the debounce scheduler (called by every state mutator that persists)
apply_log_patch("save_state schedule"
    "${_zmk_rgb}"
[==[int zmk_rgb_underglow_save_state(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    int ret = k_work_reschedule(&underglow_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    return MIN(ret, 0);
]==]
[==[int zmk_rgb_underglow_save_state(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    LOG_INF("[VPS] save_state called: scheduling save in %d ms (cur state {on=%d h=%u s=%u b=%u})",
            CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE,
            state.on, state.color.h, state.color.s, state.color.b);
    int ret = k_work_reschedule(&underglow_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    LOG_INF("[VPS] save_state: k_work_reschedule ret=%d", ret);
    return MIN(ret, 0);
]==])

# --- on() entry
apply_log_patch("on() entry"
    "${_zmk_rgb}"
[==[int zmk_rgb_underglow_on(void) {
    if (!led_strip)
        return -ENODEV;
]==]
[==[int zmk_rgb_underglow_on(void) {
    LOG_INF("[VPS] on() called: pre {on=%d h=%u s=%u b=%u}",
            state.on, state.color.h, state.color.s, state.color.b);
    if (!led_strip)
        return -ENODEV;
]==])

# --- off() entry
apply_log_patch("off() entry"
    "${_zmk_rgb}"
[==[int zmk_rgb_underglow_off(void) {
    if (!led_strip)
        return -ENODEV;
]==]
[==[int zmk_rgb_underglow_off(void) {
    LOG_INF("[VPS] off() called: pre {on=%d h=%u s=%u b=%u}",
            state.on, state.color.h, state.color.s, state.color.b);
    if (!led_strip)
        return -ENODEV;
]==])

# --- toggle()
apply_log_patch("toggle() entry"
    "${_zmk_rgb}"
[==[int zmk_rgb_underglow_toggle(void) {
    return state.on ? zmk_rgb_underglow_off() : zmk_rgb_underglow_on();
}
]==]
[==[int zmk_rgb_underglow_toggle(void) {
    LOG_INF("[VPS] toggle() called: state.on=%d -> dispatching %s",
            state.on, state.on ? "off()" : "on()");
    return state.on ? zmk_rgb_underglow_off() : zmk_rgb_underglow_on();
}
]==])

# --- set_hsb: ROOT CAUSE — stock implementation skips save_state.
# This patch combines the diagnostic logging with the actual fix: routing
# through save_state so encoder-driven brightness changes (which all funnel
# through set_hsb via behavior_rgb_underglow.c's relative-to-absolute
# conversion of RGB_BRI/RGB_BRD) finally persist to NVS.
apply_log_patch("set_hsb (logging + persistence fix)"
    "${_zmk_rgb}"
[==[int zmk_rgb_underglow_set_hsb(struct zmk_led_hsb color) {
    if (color.h > HUE_MAX || color.s > SAT_MAX || color.b > BRT_MAX) {
        return -ENOTSUP;
    }

    state.color = color;

    return 0;
}
]==]
[==[int zmk_rgb_underglow_set_hsb(struct zmk_led_hsb color) {
    LOG_INF("[VPS] set_hsb() called: incoming h=%u s=%u b=%u (will call save_state)",
            color.h, color.s, color.b);
    if (color.h > HUE_MAX || color.s > SAT_MAX || color.b > BRT_MAX) {
        LOG_WRN("[VPS] set_hsb() rejected: bounds violation");
        return -ENOTSUP;
    }

    state.color = color;

    return zmk_rgb_underglow_save_state();
}
]==])

# --- calc_brt: log what state.color.b the convert/change paths read.
# Helps diagnose the "brightness stuck at low value" report — if calc_brt
# reads a stale state.color.b (e.g. due to split sync timing) we'll see it.
apply_log_patch("calc_brt logging"
    "${_zmk_rgb}"
[==[struct zmk_led_hsb zmk_rgb_underglow_calc_brt(int direction) {
    struct zmk_led_hsb color = state.color;

    int b = color.b + (direction * CONFIG_ZMK_RGB_UNDERGLOW_BRT_STEP);
    color.b = CLAMP(b, 0, BRT_MAX);

    return color;
}
]==]
[==[struct zmk_led_hsb zmk_rgb_underglow_calc_brt(int direction) {
    struct zmk_led_hsb color = state.color;

    int b = color.b + (direction * CONFIG_ZMK_RGB_UNDERGLOW_BRT_STEP);
    color.b = CLAMP(b, 0, BRT_MAX);

    LOG_INF("[VPS] calc_brt: dir=%+d read state.b=%u step=%d -> b=%u",
            direction, state.color.b, CONFIG_ZMK_RGB_UNDERGLOW_BRT_STEP, color.b);

    return color;
}
]==])

# --- change_brt: the "happy path" — does call save_state
apply_log_patch("change_brt"
    "${_zmk_rgb}"
[==[int zmk_rgb_underglow_change_brt(int direction) {
    if (!led_strip)
        return -ENODEV;

    state.color = zmk_rgb_underglow_calc_brt(direction);

    return zmk_rgb_underglow_save_state();
}
]==]
[==[int zmk_rgb_underglow_change_brt(int direction) {
    if (!led_strip)
        return -ENODEV;

    state.color = zmk_rgb_underglow_calc_brt(direction);

    LOG_INF("[VPS] change_brt() called: direction=%d -> new b=%u (will call save_state)",
            direction, state.color.b);
    return zmk_rgb_underglow_save_state();
}
]==])

# =====================================================================
# behavior_rgb_underglow.c — see whether the binding goes through
# the relative-to-absolute conversion (which redirects to set_hsb)
# =====================================================================

# --- convert: log pre-conversion param
apply_log_patch("behavior convert pre"
    "${_zmk_beh}"
[==[static int
on_keymap_binding_convert_central_state_dependent_params(struct zmk_behavior_binding *binding,
                                                         struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
]==]
[==[static int
on_keymap_binding_convert_central_state_dependent_params(struct zmk_behavior_binding *binding,
                                                         struct zmk_behavior_binding_event event) {
    LOG_INF("[VPS] rgb_ug convert pre: param1=%d param2=0x%08x",
            binding->param1, binding->param2);
    switch (binding->param1) {
]==])

# --- convert: log post-conversion param (replaces the existing DBG-level log)
apply_log_patch("behavior convert post"
    "${_zmk_beh}"
[==[    LOG_DBG("RGB relative convert to absolute (%d/%d)", binding->param1, binding->param2);
]==]
[==[    LOG_INF("[VPS] rgb_ug convert post: param1=%d param2=0x%08x",
            binding->param1, binding->param2);
]==])

# --- binding_pressed entry: what command actually gets executed
apply_log_patch("behavior pressed"
    "${_zmk_beh}"
[==[static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
]==]
[==[static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_INF("[VPS] rgb_ug pressed: param1=%d param2=0x%08x",
            binding->param1, binding->param2);
    switch (binding->param1) {
]==])

# --- binding_released: per encoder tick we see TWO convert calls — one
# from press and one from release. The release-side convert is a "preview"
# that reads the post-press state.color.b but discards its result. Logging
# release lets us pair each convert with its actual press/release origin.
apply_log_patch("behavior released"
    "${_zmk_beh}"
[==[static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}
]==]
[==[static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_INF("[VPS] rgb_ug released: param1=%d param2=0x%08x",
            binding->param1, binding->param2);
    return ZMK_BEHAVIOR_OPAQUE;
}
]==])
