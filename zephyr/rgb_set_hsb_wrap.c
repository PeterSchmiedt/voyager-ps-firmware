/*
 * Linker wrap of zmk_rgb_underglow_set_hsb to fix the persistence bug
 * upstream in ZMK v0.3.
 *
 * Stock ZMK's set_hsb() (rgb_underglow.c:377) is:
 *     int zmk_rgb_underglow_set_hsb(struct zmk_led_hsb color) {
 *         if (color.h > HUE_MAX || color.s > SAT_MAX || color.b > BRT_MAX) {
 *             return -ENOTSUP;
 *         }
 *         state.color = color;
 *         return 0;
 *     }
 *
 * Every other state-changing entry point in the same file
 * (on/off/change_hue/change_sat/change_brt/change_spd/select_effect)
 * ends with `return zmk_rgb_underglow_save_state();`. set_hsb does not.
 * So the &rgb_ug RGB_COLOR_HSB(...) path never persists.
 *
 * Wrapping at link time means every call site in ZMK (the behavior
 * dispatch in behavior_rgb_underglow.c) ends up here instead, and we
 * forward to the real function plus the save call. CMake adds
 * -Wl,--wrap=zmk_rgb_underglow_set_hsb to the final link.
 */

#include <zmk/rgb_underglow.h>

extern int __real_zmk_rgb_underglow_set_hsb(struct zmk_led_hsb color);
extern int zmk_rgb_underglow_save_state(void);

int __wrap_zmk_rgb_underglow_set_hsb(struct zmk_led_hsb color) {
    int rc = __real_zmk_rgb_underglow_set_hsb(color);
    if (rc < 0) {
        return rc;
    }
    return zmk_rgb_underglow_save_state();
}
