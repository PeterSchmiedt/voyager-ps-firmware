/*
 * Force ZMK RGB underglow on shortly after boot.
 *
 * Reason: on this build the underglow tick handler stays silent at boot
 * regardless of CONFIG_ZMK_RGB_UNDERGLOW_ON_START=y. The most likely
 * cause is saved-state persistence — ZMK's settings_load() runs after
 * the underglow init and overwrites `state` (including state.on) from
 * NVS. Even after a settings_reset, subsequent encoder/RGB-OFF presses
 * can re-save on=false within the 60 s debounce window.
 *
 * Schedule a delayed work that fires 3 s after boot — well after
 * settings_load has finished — and calls zmk_rgb_underglow_on()
 * directly. That sets state.on=true, restarts the tick timer, and
 * kicks off the first frame to the strip.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zmk/rgb_underglow.h>

static void rgb_force_on_handler(struct k_work *work) {
    zmk_rgb_underglow_on();
}

static K_WORK_DELAYABLE_DEFINE(rgb_force_on_work, rgb_force_on_handler);

static int rgb_force_on_init(void) {
    k_work_schedule(&rgb_force_on_work, K_SECONDS(3));
    return 0;
}

SYS_INIT(rgb_force_on_init, APPLICATION, 99);
