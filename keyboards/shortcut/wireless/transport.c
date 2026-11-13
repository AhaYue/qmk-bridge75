// Copyright 2024 Su (@isuua)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "usb_main.h"
#include "transport.h"

#ifndef USB_POWER_DOWN_DELAY
#    define USB_POWER_DOWN_DELAY 7000
#endif

extern host_driver_t chibios_driver;
extern host_driver_t wireless_driver;

static transport_t transport = TRANSPORT_USB;

void wls_transport_enable(bool enable) __attribute__((weak));
void wls_transport_enable(bool enable) {
    if (enable) {
        if (host_get_driver() != &wireless_driver) {
            host_set_driver(&wireless_driver);
            // keyboard_protocol = true; // default with true
            usb_device_state_set_protocol(true);
        }
    }
}

/* Control USB device connection and disconnection by
 * controlling the power supply of the USB DP pull-up resistor.
 * Overwrite these two functions. */
void usb_power_connect(void) __attribute__((weak));
void usb_power_connect(void) {}

void usb_power_disconnect(void) __attribute__((weak));
void usb_power_disconnect(void) {}

void usb_transport_enable(bool enable) __attribute__((weak));
void usb_transport_enable(bool enable) {
    if (enable) {
        if (host_get_driver() != &chibios_driver) {
            extern bool last_suspend_state;

            /* This flag is not set to 1 with probability after usb restart */
            last_suspend_state = true;
#if !defined(KEEP_USB_CONNECTION_IN_WIRELESS_MODE)
            usb_power_connect();
            restart_usb_driver(&USBD1);
#endif
            host_set_driver(&chibios_driver);
        }
    } else {
        if (USB_DRIVER.state == USB_ACTIVE) {
            report_keyboard_t empty_report      = {0};
            report_nkro_t     empty_nkro_report = {0};
            host_keyboard_send(&empty_report);
            host_nkro_send(&empty_nkro_report);
        }

#if !defined(KEEP_USB_CONNECTION_IN_WIRELESS_MODE)
        usbStop(&USBD1);
        usbDisconnectBus(&USBD1);
        usb_power_disconnect();
#endif
    }
}

void set_transport(transport_t new_transport) {
    transport = new_transport;

    switch (transport) {
        case TRANSPORT_USB: {
            wls_transport_enable(false);
            usb_transport_enable(true);
        } break;
        case TRANSPORT_WLS: {
            usb_transport_enable(false);
            wls_transport_enable(true);
        } break;
        default:
            break;
    }
}

transport_t get_transport(void) {
    return transport;
}

// Housekeeping drives this state machine; process_action_kb no longer exists.
void usb_remote_wakeup(void) {
    static bool     suspended      = false;
    static bool     powered_down   = false;
    static bool     wake_requested = false;
    static uint32_t suspend_timer  = 0;

    if (get_transport() != TRANSPORT_USB || USB_DRIVER.state != USB_SUSPENDED) {
        // QMK's ChibiOS USB event handler already runs suspend_wakeup_init().
        suspended      = false;
        powered_down   = false;
        wake_requested = false;
        return;
    }
    if (!suspended) {
        suspended     = true;
        suspend_timer = sync_timer_read32();
    }
    if (!powered_down && sync_timer_elapsed32(suspend_timer) >= USB_POWER_DOWN_DELAY) {
        suspend_power_down();
        powered_down = true;
    }
    // Bit 1 of USB GET_STATUS is the host's remote-wakeup permission.
    if (!wake_requested && (USB_DRIVER.status & 2U) && suspend_wakeup_condition()) {
        usbWakeupHost(&USB_DRIVER);
        wake_requested = true;
#if USB_SUSPEND_WAKEUP_DELAY > 0
        wait_ms(USB_SUSPEND_WAKEUP_DELAY);
#endif
    }
}
