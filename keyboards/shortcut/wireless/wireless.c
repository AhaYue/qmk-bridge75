
// Copyright 2024 Su (@isuua)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "wireless.h"

#ifndef WLS_INQUIRY_BAT_TIME
#    define WLS_INQUIRY_BAT_TIME 3000
#endif

#ifndef WLS_KEYBOARD_REPORT_KEYS
#    define WLS_KEYBOARD_REPORT_KEYS KEYBOARD_REPORT_KEYS
#endif

static uint8_t           wls_devs = DEVS_USB;
static report_keyboard_t temp_report_keyboard;

void last_matrix_activity_trigger(void);

uint8_t wireless_keyboard_leds(void);
void    wireless_send_keyboard(report_keyboard_t *report);
void    wireless_send_nkro(report_nkro_t *report);
void    wireless_send_mouse(report_mouse_t *report);
void    wireless_send_extra(report_extra_t *report);

host_driver_t wireless_driver = {
    .keyboard_leds = wireless_keyboard_leds,
    .send_keyboard = wireless_send_keyboard,
    .send_nkro     = wireless_send_nkro,
    .send_mouse    = wireless_send_mouse,
    .send_extra    = wireless_send_extra,
};

void wireless_init(void) {
    md_init();
}

uint8_t wireless_keyboard_leds(void) __attribute__((weak));
uint8_t wireless_keyboard_leds(void) {
    if (*md_getp_state() == MD_STATE_CONNECTED) {
        return *md_getp_indicator();
    }

    return 0;
}

void wireless_send_keyboard(report_keyboard_t *report) __attribute__((weak));
void wireless_send_keyboard(report_keyboard_t *report) {
    uint8_t wls_report_kb[MD_SND_CMD_KB_LEN] = {0};

    _Static_assert((MD_SND_CMD_KB_LEN) - (WLS_KEYBOARD_REPORT_KEYS) >= 2, "WLS_KEYBOARD_REPORT_KEYS cannot be greater than MD_SND_CMD_KB_LEN - 2.");

    if (report != NULL) {
        memcpy(wls_report_kb, (uint8_t *)&report->mods, WLS_KEYBOARD_REPORT_KEYS + 2);
    }

    md_send_kb(wls_report_kb);
}

void wireless_send_nkro(report_nkro_t *report) __attribute__((weak));
void wireless_send_nkro(report_nkro_t *report) {
    uint8_t wls_report_nkro[MD_SND_CMD_NKRO_LEN] = {0};

#ifdef NKRO_ENABLE

    if (report != NULL) {
        temp_report_keyboard.mods = report->mods;
        // Release all old slots before allocating new keys; inspect all six
        // slots even after holes so held keys cannot be duplicated.
        for (uint8_t i = 0; i < WLS_KEYBOARD_REPORT_KEYS; ++i) {
            uint8_t key = temp_report_keyboard.keys[i];
            if (key / 8 >= NKRO_REPORT_BITS || !(report->bits[key / 8] & (1U << (key % 8)))) {
                temp_report_keyboard.keys[i] = 0;
            }
        }
        for (uint16_t key = 1; key < NKRO_REPORT_BITS * 8 && key <= UINT8_MAX; ++key) {
            if (!(report->bits[key / 8] & (1U << (key % 8)))) {
                continue;
            }
            uint8_t slot;
            for (slot = 0; slot < WLS_KEYBOARD_REPORT_KEYS; ++slot) {
                if (temp_report_keyboard.keys[slot] == key) {
                    break;
                }
            }
            if (slot < WLS_KEYBOARD_REPORT_KEYS) {
                continue;
            }
            for (slot = 0; slot < WLS_KEYBOARD_REPORT_KEYS; ++slot) {
                if (!temp_report_keyboard.keys[slot]) {
                    temp_report_keyboard.keys[slot] = key;
                    break;
                }
            }
            if (slot == WLS_KEYBOARD_REPORT_KEYS && key < MD_SND_CMD_NKRO_LEN * 8) {
                wls_report_nkro[key / 8] |= 1U << (key % 8);
            }
        }
    } else {
        memset(&temp_report_keyboard, 0, sizeof(temp_report_keyboard));
    }
#endif

    wireless_driver.send_keyboard(&temp_report_keyboard);
    md_send_nkro(wls_report_nkro);
}

void wireless_send_mouse(report_mouse_t *report) __attribute__((weak));
void wireless_send_mouse(report_mouse_t *report) {
    typedef struct {
        uint8_t buttons;
        int8_t  x;
        int8_t  y;
        int8_t  v;
        int8_t  h;
    } __attribute__((packed)) wls_report_mouse_t;

    wls_report_mouse_t wls_report_mouse = {0};

    if (report != NULL) {
        wls_report_mouse.buttons = report->buttons;
        wls_report_mouse.x       = report->x;
        wls_report_mouse.y       = report->y;
        wls_report_mouse.v       = report->v;
        wls_report_mouse.h       = report->h;
    }

    md_send_mouse((uint8_t *)&wls_report_mouse);
}

void wireless_send_extra(report_extra_t *report) __attribute__((weak));
void wireless_send_extra(report_extra_t *report) {
    uint16_t usage = report ? report->usage : 0;
    if (!report) {
        md_send_system((uint8_t *)&usage);
        md_send_consumer((uint8_t *)&usage);
    } else if (report->report_id == REPORT_ID_SYSTEM) {
        usage = usage >= 0x81 && usage <= 0x83 ? 1U << (usage - 0x81) : 0;
        md_send_system((uint8_t *)&usage);
    } else if (report->report_id == REPORT_ID_CONSUMER) {
        md_send_consumer((uint8_t *)&usage);
    }
}

void wireless_devs_change_user(uint8_t old_devs, uint8_t new_devs, bool reset) __attribute__((weak));
void wireless_devs_change_user(uint8_t old_devs, uint8_t new_devs, bool reset) {}

void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset) __attribute__((weak));
void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset) {}

void wireless_devs_change(uint8_t old_devs, uint8_t new_devs, bool reset) {
    bool release_old_host = wls_devs != DEVS_USB && *md_getp_state() == MD_STATE_CONNECTED;
    bool changed          = (old_devs == DEVS_USB) ? (new_devs != DEVS_USB) : (new_devs == DEVS_USB);

    // Also repair a mismatched transport flag during delayed startup restore.
    if (changed || get_transport() != ((new_devs != DEVS_USB) ? TRANSPORT_WLS : TRANSPORT_USB)) {
        set_transport((new_devs != DEVS_USB) ? TRANSPORT_WLS : TRANSPORT_USB);
    }

    if ((wls_devs != new_devs) || reset) {
        md_report_reset();
        if (release_old_host) {
            md_release_host();
        }
        memset(&temp_report_keyboard, 0, sizeof(temp_report_keyboard));
        *md_getp_state()     = MD_STATE_DISCONNECTED;
        *md_getp_indicator() = 0;
    }

    wls_devs = new_devs;
    last_matrix_activity_trigger();

    md_devs_change(new_devs, reset);
    wireless_devs_change_kb(old_devs, new_devs, reset);
    wireless_devs_change_user(old_devs, new_devs, reset);
}

uint8_t wireless_get_current_devs(void) {
    return wls_devs;
}

void wireless_pre_task(void) __attribute__((weak));
void wireless_pre_task(void) {}

void wireless_post_task(void) __attribute__((weak));
void wireless_post_task(void) {}

void wireless_task(void) {
    wireless_pre_task();
    lpwr_task();
    md_main_task();
    wireless_post_task();

    /* usb_remote_wakeup() should be invoked last so that we have chance
     * to switch to wireless after start-up when usb is not connected
     */
    usb_remote_wakeup();
    if (get_transport() == TRANSPORT_WLS && lpwr_get_state() == LPWR_NORMAL) {
        static uint32_t inqtimer = 0x00;

        if (sync_timer_elapsed32(inqtimer) >= (WLS_INQUIRY_BAT_TIME)) {
            if (md_inquire_bat()) {
                inqtimer = sync_timer_read32();
            }
        }
    }
}

void wireless_housekeeping_kb(void) __attribute__((weak));
void wireless_housekeeping_kb(void) {}

void housekeeping_task_kb(void) {
    wireless_task();
    wireless_housekeeping_kb();
}
