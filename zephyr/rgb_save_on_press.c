/*
 * Persist RGB underglow state after every key event.
 *
 * ZMK's set_hsb() (the behavior path for &rgb_ug RGB_COLOR_HSB(...)) writes the
 * new colour into RAM but never schedules an NVS save — only the toggle / on /
 * off / hue / sat / brightness-step behaviours do. So when the user presses an
 * absolute-HSB preset and then power-cycles, the change never makes it to
 * flash and the next boot loads whatever was last persisted (typically the
 * BRT_START defaults from init, since nothing has ever saved).
 *
 * Fix: subscribe to position_state_changed and call save_state on every key
 * event. ZMK's own save mechanism is debounced via CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE,
 * so this isn't an NVS write per keypress — it just bumps the timer so that
 * 2 s of keypress idleness later, whatever state.color is at that moment gets
 * flushed.
 */

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

extern int zmk_rgb_underglow_save_state(void);

static int rgb_save_on_press_listener(const zmk_event_t *eh) {
    return zmk_rgb_underglow_save_state();
}

ZMK_LISTENER(rgb_save_on_press, rgb_save_on_press_listener);
ZMK_SUBSCRIPTION(rgb_save_on_press, zmk_position_state_changed);
