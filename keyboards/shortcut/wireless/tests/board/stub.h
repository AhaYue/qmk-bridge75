#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "module.h"
#define LED_POWER_EN_PIN 1
#define BT_CABLE_PIN 2
#define BT_CHARGE_PIN 3
#define USB_POWER_EN_PIN 4
#define RGB_MATRIX_SOLID_COLOR 1
#define MD_BT_NAME "Bridge75 BT$"
enum { KC_NO, EE_CLR, BT_USB, BR_DIAG, BR_BAT, BR_SLEEP, BT_HOST1, BT_HOST2, BT_HOST3, BT_2_4G };
typedef struct { struct { bool pressed; } event; } keyrecord_t;
typedef uint32_t layer_state_t;
static layer_state_t default_layer_state = 1, saved_default_layer = 1;
static void default_layer_set(layer_state_t value) { default_layer_state = value; }
static void eeconfig_update_default_layer(layer_state_t value) { saved_default_layer = value; }
static uint32_t now = 500, saved;
static unsigned changes, names, commands, init_user, post_user;
static bool renderer_enabled, renderer_suspended = true, rail_high = true;
static uint8_t renderer_mode = 99, hsv[3] = {255, 255, 255}, current_device = DEVS_USB;
static const int usb_driver = 0, wireless_driver = 1;
static const int *active_driver;
static unsigned event_count;
static char events[32];
static void event(char value) { assert(event_count < sizeof(events)); events[event_count++] = value; }
static void eeconfig_update_kb(uint32_t value) { saved = value; }
static uint32_t eeconfig_read_kb(void) { return saved; }
static void eeconfig_init_user(void) { ++init_user; }
void eeconfig_init_kb(void);
static void eeconfig_init(void) {
    renderer_enabled = false;
    renderer_mode = 99;
    memset(hsv, 255, sizeof(hsv));
    eeconfig_init_kb();
}
static bool process_record_user(uint16_t keycode, keyrecord_t *record) { return true; }
static void keyboard_post_init_user(void) { ++post_user; }
static void gpio_write_pin_low(int pin) { if (pin == LED_POWER_EN_PIN) rail_high = false; }
static void gpio_write_pin_high(int pin) { if (pin == LED_POWER_EN_PIN) rail_high = true; }
static void gpio_set_pin_output(int pin) {}
static void gpio_set_pin_input(int pin) {}
static void gpio_set_pin_input_high(int pin) {}
static bool gpio_read_pin(int pin) { return false; }
static void rgb_matrix_enable_noeeprom(void) { renderer_enabled = true; }
static void rgb_matrix_mode_noeeprom(int mode) { renderer_mode = mode; }
static void rgb_matrix_sethsv_noeeprom(uint8_t h, uint8_t s, uint8_t v) { hsv[0] = h; hsv[1] = s; hsv[2] = v; }
static void rgb_matrix_set_suspend_state(bool value) { renderer_suspended = value; }
static void wireless_init(void) { event('I'); }
void wireless_devs_change_kb(uint8_t old_device, uint8_t new_device, bool reset);
static void wireless_devs_change(uint8_t old_device, uint8_t new_device, bool reset) {
    assert(!reset); ++changes; event('D'); current_device = new_device;
    active_driver = new_device == DEVS_USB ? &usb_driver : &wireless_driver;
    wireless_devs_change_kb(old_device, new_device, reset);
}
static uint8_t wireless_get_current_devs(void) { return current_device; }
static void md_send_devinfo(const char *name) { assert(strcmp(name, MD_BT_NAME) == 0); ++names; event('N'); }
void md_send_devctrl(uint8_t command) { ++commands; event('C'); }
static uint32_t timer_read32(void) { return now; }
static uint32_t timer_elapsed32(uint32_t start) { return now - start; }
