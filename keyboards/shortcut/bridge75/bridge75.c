// Copyright 2025 emolitor (github.com/emolitor)
// Copyright 2024 Westberry Technology (ChangZhou) Corp., Ltd
// Copyright 2024 Wind (@yelishang)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "bridge75.h"
#include <stdio.h>
#include "send_string.h"

#ifdef WIRELESS_ENABLE
#    include "wireless.h"

typedef union {
    uint32_t raw;
    struct {
        uint8_t devs : 3;
        bool    module_sleep : 1;
        uint8_t reserved : 4;
        uint8_t version;
    };
} confinfo_t;
confinfo_t confinfo;

uint32_t    post_init_timer = 0x00;
static bool startup_pending = true;
static bool module_config_pending;

uint8_t bat_level   = 0;
uint8_t blink_index = 0;
bool    blink_fast  = true;
bool    blink_slow  = true;

// Expose md_send_devinfo to support the Bridge75 Bluetooth naming quirk
// The module substitutes $ with the selected Bluetooth host number.
void md_send_devinfo(const char *name);

// Pending module commands must be drained before MCU STOP.
bool smsg_is_busy(void);

void eeconfig_init_kb(void) {
    confinfo.raw     = 0;
    confinfo.devs    = DEVS_USB;
    confinfo.version = 0xB7;
    eeconfig_update_kb(confinfo.raw);
    eeconfig_init_user();
}

static void configure_status_leds(void) {
    // These LEDs are status indicators. Old RGB EEPROM settings must not hide
    // pairing feedback, and the base color should stay black.
    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    rgb_matrix_sethsv_noeeprom(0, 0, 0);
    rgb_matrix_set_suspend_state(false);
}

void keyboard_post_init_kb(void) {
    confinfo.raw = eeconfig_read_kb();
    if (confinfo.version != 0xB7 || (confinfo.devs > DEVS_BT3 && confinfo.devs != DEVS_2G4)) {
        eeconfig_init_kb();
    }

    // Only base layers 0 (Windows) and 2 (Mac) are persistent layouts here.
    if (default_layer_state != 1U && default_layer_state != (1U << 2)) {
        default_layer_set(1U);
        eeconfig_update_default_layer(1U);
    }

    gpio_write_pin_low(LED_POWER_EN_PIN);
    gpio_set_pin_output(LED_POWER_EN_PIN);

    // Set GPIO as high input for battery charging state
    gpio_set_pin_input(BT_CABLE_PIN);
    gpio_set_pin_input_high(BT_CHARGE_PIN);

    // Set USB_POWER_EN_PIN state before enabling the output to avoid instability
    if (confinfo.devs == DEVS_USB && gpio_read_pin(BT_CABLE_PIN)) {
        gpio_write_pin_low(USB_POWER_EN_PIN);
    } else {
        gpio_write_pin_high(USB_POWER_EN_PIN);
    }
    gpio_set_pin_output(USB_POWER_EN_PIN);

    configure_status_leds();

    wireless_init();
    // protocol_post_init() runs after this hook and installs the USB driver.
    // Restore the selected transport in the first main-loop service instead.
    startup_pending = true;

    keyboard_post_init_user();
}

void usb_power_connect(void) {
    gpio_write_pin_low(USB_POWER_EN_PIN);
    wait_ms(5);
}

void usb_power_disconnect(void) {
    gpio_write_pin_high(USB_POWER_EN_PIN);
}

void suspend_power_down_kb(void) {
    gpio_write_pin_high(LED_POWER_EN_PIN);
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    gpio_write_pin_low(LED_POWER_EN_PIN);

    if (wireless_get_current_devs() == DEVS_USB) {
        wireless_devs_change(DEVS_USB, DEVS_USB, false);
    }
    suspend_wakeup_init_user();
    wait_ms(5);
}

bool lpwr_is_allow_timeout_hook(void) {
    if (smsg_is_busy() || gpio_read_pin(BT_CABLE_PIN)) {
        return false;
    }

    return true;
}

void md_recovery_kb(void) {
    md_send_devinfo(MD_BT_NAME);
    post_init_timer       = timer_read32();
    module_config_pending = true;
}

static void apply_module_sleep(void) {
    md_send_devctrl(confinfo.module_sleep ? MD_SND_CMD_DEVCTRL_SLEEP_BT_EN : MD_SND_CMD_DEVCTRL_SLEEP_BT_DIS);
    md_send_devctrl(confinfo.module_sleep ? MD_SND_CMD_DEVCTRL_SLEEP_2G4_EN : MD_SND_CMD_DEVCTRL_SLEEP_2G4_DIS);
}

void wireless_post_task(void) {
    if (startup_pending) {
        startup_pending = false;
        wireless_devs_change(!confinfo.devs, confinfo.devs, false);
        // The device-change callback queues the name and configuration after
        // the old device's TX queue has been cleared.
    }
    if (module_config_pending && timer_elapsed32(post_init_timer) >= 100) {
        md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION); // get the module fw version.
        apply_module_sleep();
        module_config_pending = false;
    }
}

void md_devs_change(uint8_t devs, bool reset) {
    switch (devs) {
        case DEVS_USB: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
        } break;
        case DEVS_2G4: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_2G4);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT1: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT1);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT2: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT2);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT3: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT3);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        default:
            break;
    }
}

#ifndef BRIDGE75_BUILD
#    define BRIDGE75_BUILD "dev"
#endif

static uint32_t indicator_frames;

// Text typed out through send_char(). Both Fn+D (diagnostics) and the battery
// key share this buffer so they cannot interleave.
typedef enum { OUTPUT_NONE, OUTPUT_DIAG, OUTPUT_BAT } output_kind_t;

static output_kind_t output_kind;
static bool          output_pending;
static char          output_text[320];
static uint16_t      output_pos;
static uint32_t      output_timer;
static uint16_t pairing_key = KC_NO;
static uint8_t  pairing_device;
static uint32_t pairing_timer;
static bool     pairing_sent;

// Emit one character per task interval so UART ACKs continue to be processed.
void wireless_housekeeping_kb(void) {
    if (pairing_key != KC_NO && !pairing_sent && timer_elapsed32(pairing_timer) >= 1000) {
        pairing_sent = true;
        wireless_devs_change(wireless_get_current_devs(), pairing_device, true);
    }
    bool keys_down = false;
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row)
        keys_down |= matrix_get_row(row) != 0;
    bool ready = wireless_get_current_devs() == DEVS_USB || *md_getp_state() == MD_STATE_CONNECTED;
    if (output_pending && !keys_down && ready && !get_mods() && !get_weak_mods() && !get_oneshot_mods()) {
        if (output_kind == OUTPUT_DIAG) {
            snprintf(output_text, sizeof(output_text), "bridge75-" BRIDGE75_BUILD " fw=%u dev=%u state=%u wakes=%lu source=%u retries=%lu drops=%lu sleep=%u bat=%u rx=%lu ack=%lu valid=%lu bad=%lu tx=%lu rgb=%u/%u/%u leds=%lu rail=%u", md_get_version(), wireless_get_current_devs(), *md_getp_state(), (unsigned long)lpwr_get_wakeup_count(), lpwr_get_sleep_wakeupcd(), (unsigned long)md_get_retry_count(), (unsigned long)md_get_drop_count(), confinfo.module_sleep, *md_getp_bat(), (unsigned long)md_get_rx_bytes(), (unsigned long)md_get_ack_count(), (unsigned long)md_get_valid_count(), (unsigned long)md_get_bad_count(), (unsigned long)md_get_tx_count(), rgb_matrix_is_enabled(), rgb_matrix_get_mode(), rgb_matrix_get_suspend_state(), (unsigned long)indicator_frames, (unsigned int)gpio_read_pin(LED_POWER_EN_PIN));
        } else if (output_kind == OUTPUT_BAT) {
            snprintf(output_text, sizeof(output_text), "bat=%u", (unsigned int)*md_getp_bat());
        } else {
            output_text[0] = '\0';
        }
        output_kind    = OUTPUT_NONE;
        output_pending = false;
        output_pos     = 0;
        output_timer   = timer_read32();
    }
    if (output_text[output_pos] && !keys_down && ready && !get_mods() && !get_weak_mods() && !get_oneshot_mods() && timer_elapsed32(output_timer) >= 30) {
        send_char(output_text[output_pos++]);
        output_timer = timer_read32();
    }
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (process_record_user(keycode, record) != true) {
        return false;
    }

    switch (keycode) {
        case EE_CLR: {
            // Only reset the eeprom on keypress to avoid repeating eeprom
            // clear if held down.
            if (record->event.pressed) {
                eeconfig_init();
                configure_status_leds();
                pairing_key = KC_NO;
                wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);
            }
            return false;
        }
        case BT_USB: {
            if (record->event.pressed) {
                pairing_key = KC_NO;
                wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);
            }
            return false;
        }
        case BR_DIAG:
            if (record->event.pressed) {
                output_kind    = OUTPUT_DIAG;
                output_pending = true;
            }
            return false;
        case BR_BAT:
            if (record->event.pressed) {
                output_kind    = OUTPUT_BAT;
                output_pending = true;
            }
            return false;
        case BR_SLEEP:
            if (record->event.pressed) {
                confinfo.module_sleep = !confinfo.module_sleep;
                eeconfig_update_kb(confinfo.raw);
                apply_module_sleep();
            }
            return false;
        case BT_HOST1:
        case BT_HOST2:
        case BT_HOST3:
        case BT_2_4G:
            if (record->event.pressed) {
                pairing_key    = keycode;
                pairing_device = keycode == BT_HOST1 ? DEVS_BT1 : keycode == BT_HOST2 ? DEVS_BT2 : keycode == BT_HOST3 ? DEVS_BT3 : DEVS_2G4;
                pairing_timer  = timer_read32();
                pairing_sent   = false;
                wireless_devs_change(wireless_get_current_devs(), pairing_device, false);
            } else if (pairing_key == keycode) {
                pairing_key = KC_NO;
            }
            return false;
    }

    return true;
}

void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset) {
    if (old_devs != new_devs || reset) {
        output_kind               = OUTPUT_NONE;
        output_pending            = false;
        output_text[output_pos]   = '\0';
    }
    // USB suspend can disable both the LED rail and renderer. Mode selection
    // must restore status feedback without clearing the current key/layer state.
    gpio_write_pin_low(LED_POWER_EN_PIN);
    rgb_matrix_enable_noeeprom();
    rgb_matrix_set_suspend_state(false);
    md_recovery_kb();
    if (confinfo.devs != wireless_get_current_devs()) {
        confinfo.devs = wireless_get_current_devs();
        eeconfig_update_kb(confinfo.raw);
    }
}

void blink(uint8_t key_index, uint8_t r, uint8_t g, uint8_t b, bool blink) {
    if (blink) {
        rgb_matrix_set_color(key_index, r, g, b);
    } else {
        rgb_matrix_set_color(key_index, RGB_OFF);
    }
}

void connection_indicators(void) {
    switch (confinfo.devs) {
        case DEVS_BT1: {
            if (*md_getp_state() == MD_STATE_PAIRING) {
                blink(DEVS_BT1_INDEX, RGB_ADJ_WHITE, blink_fast);
            } else if (*md_getp_state() != MD_STATE_CONNECTED) {
                blink(DEVS_BT1_INDEX, RGB_ADJ_WHITE, blink_slow);
            } else {
                rgb_matrix_set_color(DEVS_BT1_INDEX, RGB_ADJ_WHITE);
            }
        } break;
        case DEVS_BT2: {
            if (*md_getp_state() == MD_STATE_PAIRING) {
                blink(DEVS_BT2_INDEX, RGB_ADJ_WHITE, blink_fast);
            } else if (*md_getp_state() != MD_STATE_CONNECTED) {
                blink(DEVS_BT2_INDEX, RGB_ADJ_WHITE, blink_slow);
            } else {
                rgb_matrix_set_color(DEVS_BT2_INDEX, RGB_ADJ_WHITE);
            }
        } break;
        case DEVS_BT3: {
            if (*md_getp_state() == MD_STATE_PAIRING) {
                blink(DEVS_BT3_INDEX, RGB_ADJ_WHITE, blink_fast);
            } else if (*md_getp_state() != MD_STATE_CONNECTED) {
                blink(DEVS_BT3_INDEX, RGB_ADJ_WHITE, blink_slow);
            } else {
                rgb_matrix_set_color(DEVS_BT3_INDEX, RGB_ADJ_WHITE);
            }
        } break;
        case DEVS_2G4: {
            if (*md_getp_state() == MD_STATE_PAIRING) {
                blink(DEVS_2G4_INDEX, RGB_ADJ_WHITE, blink_fast);
            } else if (*md_getp_state() != MD_STATE_CONNECTED) {
                blink(DEVS_2G4_INDEX, RGB_ADJ_WHITE, blink_slow);
            } else {
                rgb_matrix_set_color(DEVS_2G4_INDEX, RGB_ADJ_WHITE);
            }
        } break;
    }
}

void battery_percent_changed_kb(uint8_t level) {
    bat_level = level;
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    ++indicator_frames;
    uint8_t current_layer = get_highest_layer(default_layer_state | layer_state);
    blink_index           = blink_index + 1;
    blink_fast            = (blink_index % 64 == 0) ? !blink_fast : blink_fast;
    blink_slow            = (blink_index % 128 == 0) ? !blink_slow : blink_slow;

    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) {
        return false;
    }

    // When in Layer 1 show the UX
    if ((current_layer == 1) || (current_layer == 3)) {
        // Set all mapped keys to orange
        uint8_t layer = get_highest_layer(layer_state);
        for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                uint8_t index = g_led_config.matrix_co[row][col];

                if (index >= led_min && index < led_max && index != NO_LED && keymap_key_to_keycode(layer, (keypos_t){col, row}) > KC_TRNS) {
                    if (current_layer == 3) {
                        rgb_matrix_set_color(index, RGB_ADJ_BLUE);
                    } else {
                        rgb_matrix_set_color(index, RGB_ADJ_ORANGE);
                    }
                }
            }
        }

        if (gpio_read_pin(BT_CABLE_PIN) && !gpio_read_pin(BT_CHARGE_PIN)) {
            // Check if we are plugged in and charging
            rgb_matrix_set_color(ESCAPE_INDEX, RGB_ADJ_WHITE);
        } else {
            if (bat_level > 90) {
                rgb_matrix_set_color(ESCAPE_INDEX, RGB_ADJ_GREEN);
            } else if (bat_level > 50) {
                rgb_matrix_set_color(ESCAPE_INDEX, RGB_ADJ_BLUE);
            } else if (bat_level > 10) {
                rgb_matrix_set_color(ESCAPE_INDEX, RGB_ADJ_YELLOW);
            } else if (bat_level > 0) {
                rgb_matrix_set_color(ESCAPE_INDEX, RGB_ADJ_RED);
            } else {
                // Keep Fn feedback visible even before the module reports battery.
                rgb_matrix_set_color(ESCAPE_INDEX, RGB_ADJ_BLUE);
            }
        }

        // Show active connection
        connection_indicators();
    } else if (confinfo.devs != DEVS_USB && *md_getp_state() != MD_STATE_CONNECTED) {
        // Always show wireless connection indicators when not connected
        connection_indicators();
    }

    return true;
}

#endif

bool rgb_matrix_indicators_kb(void) {
    if (!rgb_matrix_indicators_user()) {
        return false;
    }
    if (host_keyboard_led_state().caps_lock) {
        rgb_matrix_set_color(CAPSLOCK_INDEX, 0xFF, 0xFF, 0xFF);
    }
    return true;
}
