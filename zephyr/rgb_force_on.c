/*
 * Force ZMK RGB underglow on shortly after boot, AND visibly indicate via
 * the onboard blue LED that this code is actually executing.
 *
 * Diagnostic: this file's job is to make it observable from outside whether
 * our custom module is being compiled and run. After +1.5 s the blue LED on
 * P0.15 turns on; after +3 s, zmk_rgb_underglow_on() is called and the LED
 * strip should start updating (if the underglow path is sound).
 *
 * If the blue LED never turns on -> this module isn't being built/linked.
 * If blue LED turns on but D0 still stays 0V -> zmk_rgb_underglow_on() is
 * being called but the underglow driver is silently failing (led_strip is
 * NULL or led_strip_update_rgb returns ENODEV every tick).
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zmk/rgb_underglow.h>

/* Blue LED on the nice_nano_v2 is gpio0 pin 15, active high. */
#define BLUE_LED_NODE DT_NODELABEL(blue_led)

static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BLUE_LED_NODE, gpios);

static void blue_led_on_handler(struct k_work *work) {
    gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE);
}

static void rgb_force_on_handler(struct k_work *work) {
    zmk_rgb_underglow_on();
}

static K_WORK_DELAYABLE_DEFINE(blue_led_on_work, blue_led_on_handler);
static K_WORK_DELAYABLE_DEFINE(rgb_force_on_work, rgb_force_on_handler);

static int rgb_force_on_init(void) {
    k_work_schedule(&blue_led_on_work, K_MSEC(1500));
    k_work_schedule(&rgb_force_on_work, K_MSEC(3000));
    return 0;
}

SYS_INIT(rgb_force_on_init, APPLICATION, 99);
