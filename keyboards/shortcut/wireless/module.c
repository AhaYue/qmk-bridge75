// Copyright 2024 Su (@isuua)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "module.h"
#include "smsg.h"
#include "uart.h"

uint8_t wireless_get_current_devs(void);

#ifndef MD_BAUD_RATE
#    define MD_BAUD_RATE 115200
#endif

// Observed module ACKs can arrive after 10 ms; a short timeout only creates
// duplicate transmissions whose ACKs then have to be reconciled.
#ifndef MD_SNED_PKT_TIMEOUT
#    define MD_SNED_PKT_TIMEOUT 30
#endif

// ACKs owed for already-completed frames are discarded until this quiet window ends.
#ifndef MD_STALE_ACK_WINDOW
#    define MD_STALE_ACK_WINDOW 100
#endif

#ifndef MD_SEND_PKT_RETRY
#    define MD_SEND_PKT_RETRY 40
#endif

#ifndef MD_SEND_PKT_PAYLOAD_MAX
#    define MD_SEND_PKT_PAYLOAD_MAX 50
#endif

// While disconnected the saved host is reselected, first after 500 ms and then
// at doubling intervals up to 10 s, so an absent host does not keep the UART
// busy and the MCU out of STOP. New input or a reset returns to the short delay.
#ifndef MD_RECONNECT_MIN
#    define MD_RECONNECT_MIN 500
#endif

#ifndef MD_RECONNECT_MAX
#    define MD_RECONNECT_MAX 10000
#endif

#ifndef MD_BT1_NAME
#    define MD_BT1_NAME PRODUCT " BT1"
#endif

#ifndef MD_BT2_NAME
#    define MD_BT2_NAME PRODUCT " BT2"
#endif

#ifndef MD_BT3_NAME
#    define MD_BT3_NAME PRODUCT " BT3"
#endif

#ifndef MD_BT4_NAME
#    define MD_BT4_NAME PRODUCT " BT4"
#endif

#ifndef MD_BT5_NAME
#    define MD_BT5_NAME PRODUCT " BT5"
#endif

#ifndef MD_DONGLE_MANUFACTURER
#    define MD_DONGLE_MANUFACTURER MANUFACTURER
#endif

#ifndef MD_DONGLE_PRODUCT
#    define MD_DONGLE_PRODUCT PRODUCT " Dongle"
#endif

#ifndef MD_RAW_SIZE
#    define MD_RAW_SIZE 32
#endif

#define USBCONCAT(a, b) a##b
#define USBSTR(s) USBCONCAT(L, s)

typedef struct {
    uint8_t state;
    uint8_t indicator;
    uint8_t version;
    uint8_t bat;
} md_info_t;

static uint8_t md_pkt_payload[MD_SEND_PKT_PAYLOAD_MAX] = {0};
static uint8_t md_rev_payload[MD_SEND_PKT_PAYLOAD_MAX] = {0};
static uint8_t md_raw_payload[MD_RAW_SIZE]             = {0};

static uint32_t smsg_timer, rx_timer, retry_count, drop_count, reconnect_timer;
static uint8_t  smsg_retry, rx_count, rx_expected;
// Module ACKs carry no sequence number. Every transmission of a frame earns one
// ACK, so after the first ACK completes a frame its retransmissions still owe
// ACKs. Consuming those first can cause a harmless retransmission of the next
// frame, instead of crediting it with an ACK it never earned.
// If the module never sent an owed ACK (that copy was lost), the frame in flight
// has its own ACK consumed and times out. Its retransmission would then owe an
// ACK of its own and the same thing would repeat for every frame in a burst.
// A timeout after a consumed ACK therefore counts that ACK as this frame's.
static uint8_t  frame_sends, stale_acks;
static bool     frame_ate_ack;
static uint32_t stale_ack_timer;
static bool     reconnect_pending;
static uint32_t reconnect_interval = MD_RECONNECT_MIN;
static uint32_t rx_bytes, rx_ack_count, rx_valid_count, rx_bad_count, tx_packet_count;

static md_info_t md_info = {
    .bat       = 100,
    .indicator = 0,
    .version   = 0,
    .state     = MD_STATE_NONE,
};

static void md_send_ack(void) {
    uint8_t sdata[0x03] = {0x61, 0x0D, 0x0A};
    uart_transmit(sdata, sizeof(sdata));
}

static bool md_check_sum(const uint8_t *data, uint32_t length) {
    uint8_t sum = 0;

    for (uint32_t i = 0; i < (length - 1); i++) {
        sum += data[i];
    }

    return sum == data[length - 1];
}

static void md_calc_check_sum(uint8_t *data, uint32_t length) {
    uint8_t sum = 0;

    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }

    data[length] = sum;
}

// Replay only recent report transitions. Snapshots survive overflow and expiry so
// that a dropped key release cannot leave the host with a stuck key.
#define MD_REPORT_COUNT 16
#define MD_REPORT_FRAME_MAX (MD_SND_CMD_NKRO_LEN + 2)
typedef struct {
    uint32_t time;
    uint8_t  length;
    uint8_t  data[MD_REPORT_FRAME_MAX];
} md_report_t;
static md_report_t reports[MD_REPORT_COUNT], latest[6];
static uint8_t     report_head, report_count;
static uint8_t     resync_mask;

static uint8_t md_report_index(uint8_t cmd) {
    switch (cmd) {
        case MD_SND_CMD_SEND_KB:
            return 0;
        case MD_SND_CMD_SEND_NKRO:
            return 1;
        case MD_SND_CMD_SEND_CONSUMER:
            return 2;
        case MD_SND_CMD_SEND_SYSTEM:
            return 3;
        case MD_SND_CMD_SEND_FN:
            return 4;
        default:
            return 5;
    }
}

static void md_ack_reset(void) {
    frame_sends = stale_acks = 0;
    frame_ate_ack            = false;
}

void md_report_reset(void) {
    smsg_init();
    smsg_retry        = 0;
    md_ack_reset();
    reconnect_pending = false;
    report_head = report_count = resync_mask = 0;
    reconnect_interval                       = MD_RECONNECT_MIN;
    memset(latest, 0, sizeof(latest));
}

void md_report_resync(void) {
    for (uint8_t i = 0; i < 6; ++i) {
        if (latest[i].length) {
            resync_mask |= 1U << i;
        }
    }
}

static void md_report_submit(uint8_t *data, uint8_t length) {
    uint8_t     index  = md_report_index(data[0]);
    md_report_t report = {.time = sync_timer_read32(), .length = length};
    memcpy(report.data, data, length);
    if (md_info.state != MD_STATE_CONNECTED) {
        reconnect_interval = MD_RECONNECT_MIN; // the user is typing: reconnect promptly
    }
    latest[index] = report;
    if (index == 5) {
        // Mouse deltas are relative; recovery must only synchronize buttons.
        memset(&latest[index].data[2], 0, MD_SND_CMD_MOUSE_LEN - 1);
        md_calc_check_sum(latest[index].data, length - 1);
    }
    if (report_count == MD_REPORT_COUNT) {
        report_head = (report_head + 1) % MD_REPORT_COUNT;
        report_count--;
        drop_count++;
        md_report_resync();
    }
    reports[(report_head + report_count) % MD_REPORT_COUNT] = report;
    report_count++;
}

// Queue releases before the device-selection command. The module ACK protocol
// preserves this order; delivery still depends on the old radio link existing.
void md_release_host(void) {
    static const uint8_t commands[] = {MD_SND_CMD_SEND_KB, MD_SND_CMD_SEND_NKRO, MD_SND_CMD_SEND_CONSUMER, MD_SND_CMD_SEND_SYSTEM, MD_SND_CMD_SEND_FN, MD_SND_CMD_SEND_MOUSE};
    static const uint8_t sizes[]    = {MD_SND_CMD_KB_LEN, MD_SND_CMD_NKRO_LEN, MD_SND_CMD_CONSUMER_LEN, MD_SND_CMD_SYSTEM_LEN, MD_SND_CMD_FN_LEN, MD_SND_CMD_MOUSE_LEN};
    for (uint8_t i = 0; i < sizeof(commands); ++i) {
        uint8_t frame[MD_REPORT_FRAME_MAX] = {0};
        frame[0]                           = commands[i];
        md_calc_check_sum(frame, sizes[i] + 1);
        if (!smsg_push(frame, sizes[i] + 2)) {
            drop_count++;
        }
    }
}

static void md_report_task(void) {
    while (report_count && sync_timer_elapsed32(reports[report_head].time) > 5000) {
        report_head = (report_head + 1) % MD_REPORT_COUNT;
        report_count--;
        drop_count++;
        md_report_resync();
    }
    if (md_info.state != MD_STATE_CONNECTED) {
        if (wireless_get_current_devs() != DEVS_USB && (reconnect_pending || report_count || resync_mask) && md_info.state != MD_STATE_PAIRING && sync_timer_elapsed32(reconnect_timer) >= reconnect_interval && !smsg_is_busy()) {
            reconnect_timer    = sync_timer_read32();
            reconnect_interval = reconnect_interval >= MD_RECONNECT_MAX / 2 ? MD_RECONNECT_MAX : reconnect_interval * 2;
            md_devs_change(wireless_get_current_devs(), false);
            if (reconnect_pending) md_recovery_kb();
        }
        return;
    }
    while (report_count) {
        md_report_t *report = &reports[report_head];
        if (!smsg_push(report->data, report->length)) {
            return;
        }
        report_head = (report_head + 1) % MD_REPORT_COUNT;
        report_count--;
    }
    for (uint8_t i = 0; i < 6; ++i) {
        if (resync_mask & (1U << i)) {
            if (!smsg_push(latest[i].data, latest[i].length)) {
                return;
            }
            resync_mask &= ~(1U << i);
        }
    }
}

uint32_t md_get_rx_bytes(void) {
    return rx_bytes;
}
uint32_t md_get_ack_count(void) {
    return rx_ack_count;
}
uint32_t md_get_valid_count(void) {
    return rx_valid_count;
}
uint32_t md_get_bad_count(void) {
    return rx_bad_count;
}
uint32_t md_get_tx_count(void) {
    return tx_packet_count;
}

uint32_t md_get_retry_count(void) {
    return retry_count;
}
uint32_t md_get_drop_count(void) {
    return drop_count;
}
bool md_reports_pending(void) {
    return report_count || (resync_mask && md_info.state == MD_STATE_CONNECTED);
}

void md_recovery_kb(void) __attribute__((weak));
void md_recovery_kb(void) {}

void md_recover(void) {
    md_init();
    reconnect_interval = MD_RECONNECT_MIN;
    md_info.state     = MD_STATE_DISCONNECTED;
    reconnect_pending = true;
    md_report_resync();
    md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);
    md_devs_change(wireless_get_current_devs(), false);
    md_recovery_kb();
}

bool md_receive_process_user(uint8_t *pdata, uint8_t len) __attribute__((weak));
bool md_receive_process_user(uint8_t *pdata, uint8_t len) {
    return true;
}

bool md_receive_process_kb(uint8_t *pdata, uint8_t len) __attribute__((weak));
bool md_receive_process_kb(uint8_t *pdata, uint8_t len) {
    return md_receive_process_user(pdata, len);
}

void md_receive_raw_cb(uint8_t *pdata, uint8_t len) __attribute__((weak));
void md_receive_raw_cb(uint8_t *pdata, uint8_t len) {}

void md_receive_host_cb(bool resume) __attribute__((weak));
void md_receive_host_cb(bool resume) {}

static void md_receive_msg_task(void) {
    if (stale_acks && sync_timer_elapsed32(stale_ack_timer) > MD_STALE_ACK_WINDOW) {
        stale_acks = 0;
    }
    if (rx_count && sync_timer_elapsed32(rx_timer) > 20) {
        rx_count = 0;
        drop_count++;
    }
    while (uart_available()) {
        uint8_t data = uart_read();
        ++rx_bytes;
        rx_timer = sync_timer_read32();
        if (!rx_count) {
            switch (data) {
                case MD_REV_CMD_RAW:
                    rx_expected = MD_RAW_SIZE + 2;
                    break;
                case MD_REV_CMD_INDICATOR:
                case MD_REV_CMD_DEVCTRL:
                case MD_REV_CMD_BATVOL:
                case MD_REV_CMD_MD_FW_VERSION:
                case MD_REV_CMD_HOST_STATE:
                case 0x61:
                    rx_expected = 3;
                    break;
                default:
                    continue;
            }
        }
        if (rx_count >= sizeof(md_rev_payload) || rx_expected > sizeof(md_rev_payload)) {
            rx_count = 0;
            drop_count++;
            continue;
        }
        md_rev_payload[rx_count++] = data;
        if (rx_count < rx_expected) {
            continue;
        }
        uint8_t data_count = rx_count;
        rx_count           = 0; // callbacks may consume this frame; always release parser first.
        if (md_rev_payload[0] == 0x61) {
            if (md_rev_payload[1] != 0x0D || data != 0x0A) {
                continue;
            }
            smsg_states_t state = smsg_get_state();
            if (stale_acks) {
                --stale_acks;
                stale_ack_timer = sync_timer_read32();
                if (state == smsg_state_busy || state == smsg_state_retry) {
                    frame_ate_ack = true;
                }
                continue;
            }
            if (state == smsg_state_busy || state == smsg_state_retry) {
                ++rx_ack_count;
                smsg_set_state(smsg_state_replied);
                if (frame_sends > 1) {
                    stale_acks      = frame_sends - 1;
                    stale_ack_timer = sync_timer_read32();
                }
                frame_sends   = 0;
                frame_ate_ack = false;
            }
            continue;
        }
        if (md_check_sum(md_rev_payload, data_count)) {
            ++rx_valid_count;
            md_send_ack();

            if (md_receive_process_kb(md_rev_payload, data_count) != true) {
                return;
            }

            switch (md_rev_payload[0]) {
                case MD_REV_CMD_RAW: {
                    uint8_t *pdata;
                    pdata = &md_rev_payload[1];

                    memcpy(md_raw_payload, pdata, 32);
                    md_receive_raw_cb(md_raw_payload, 32);

                } break;
                case MD_REV_CMD_INDICATOR: {
                    md_info.indicator = md_rev_payload[1];
                } break;
                case MD_REV_CMD_DEVCTRL: {
                    switch (md_rev_payload[1]) {
                        case MD_REV_CMD_DEVCTRL_PAIRING: {
                            md_info.state = MD_STATE_PAIRING;
                        } break;
                        case MD_REV_CMD_DEVCTRL_CONNECTED: {
                            if (md_info.state != MD_STATE_CONNECTED) {
                                md_report_resync();
                            }
                            reconnect_pending  = false;
                            reconnect_interval = MD_RECONNECT_MIN;
                            md_info.state      = MD_STATE_CONNECTED;
                        } break;
                        case MD_REV_CMD_DEVCTRL_DISCONNECTED: {
                            md_info.state = MD_STATE_DISCONNECTED;
                        } break;
                        case MD_REV_CMD_DEVCTRL_REJECT: {
                            md_info.state = MD_STATE_REJECT;
                        } break;
                        default:
                            break;
                    }
                } break;
                case MD_REV_CMD_BATVOL: {
                    md_info.bat = md_rev_payload[1];
                } break;
                case MD_REV_CMD_MD_FW_VERSION: {
                    md_info.version = md_rev_payload[1];
                } break;
                case MD_REV_CMD_HOST_STATE: {
                    md_receive_host_cb(md_rev_payload[1] == MD_REV_CMD_HOST_STATE_RESUME);
                } break;
                default:
                    break;
            }
        } else {
            ++rx_bad_count;
        }
    }
}

static void md_send_pkt_task(void) {
    switch (smsg_get_state()) {
        case smsg_state_busy: {
            if (sync_timer_elapsed32(smsg_timer) > (MD_SNED_PKT_TIMEOUT)) {
                if (frame_ate_ack && frame_sends) {
                    // The consumed ACK was most likely this frame's own.
                    --frame_sends;
                    frame_ate_ack = false;
                }
                smsg_set_state(smsg_state_retry);
            }
        } break;
        case smsg_state_retry: {
            retry_count++;
            if (++smsg_retry > MD_SEND_PKT_RETRY) {
                // Clear stale traffic; a later service pass reselects the saved host.
                drop_count++;
                smsg_init();
                smsg_retry        = 0;
                md_ack_reset();
                md_info.state     = MD_STATE_DISCONNECTED;
                reconnect_timer    = sync_timer_read32();
                reconnect_interval = MD_RECONNECT_MIN;
                reconnect_pending  = true;
                md_report_resync();
            }
            smsg_set_state(smsg_state_free);
        } break;
        case smsg_state_replied: {
            smsg_retry = 0;
            smsg_pop();
            smsg_set_state(smsg_state_free);
            // Send the next queued frame in the same pass.
            __attribute__((fallthrough));
        }
        case smsg_state_free: {
            uint32_t size = smsg_peek(md_pkt_payload);
            if (size) {
                if (md_send_pkt(md_pkt_payload, size)) {
                    if (frame_sends < UINT8_MAX) {
                        ++frame_sends;
                    }
                    smsg_timer = sync_timer_read32();
                    smsg_set_state(smsg_state_busy);
                } else {
                    smsg_set_state(smsg_state_replied);
                }
            }
        } break;
        default:
            break;
    }
}

void md_init(void) {
    uart_init(MD_BAUD_RATE);
    smsg_init();
    smsg_retry = rx_count = 0;
    reconnect_pending     = false;
    md_ack_reset();

    memset(md_pkt_payload, 0, sizeof(md_pkt_payload));
}

void md_main_task(void) {
    md_receive_msg_task();
    md_send_pkt_task();
    md_report_task();
}

uint8_t *md_getp_state(void) {
    return &md_info.state;
}

uint8_t *md_getp_bat(void) {
    return &md_info.bat;
}

uint8_t *md_getp_indicator(void) {
    return &md_info.indicator;
}

uint8_t md_get_version(void) {
    return md_info.version;
}

bool md_send_pkt(uint8_t *data, uint32_t len) {
    bool retval = true;

    if (!data || !len) {
        return false;
    }

    // switch (*data) {
    //     case MD_SND_CMD_RAW: {
    //         retval = false;
    //     } break;
    //     default:
    //         break;
    // }

    // send
    uart_transmit(data, len);
    ++tx_packet_count;

    return retval;
}

void md_send_kb(uint8_t *data) {
    uint8_t sdata[MD_SND_CMD_KB_LEN + 2] = {0x00};

    sdata[0] = MD_SND_CMD_SEND_KB;
    memcpy(&sdata[1], data, sizeof(sdata) - 2);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    md_report_submit(sdata, sizeof(sdata));
}

void md_send_nkro(uint8_t *data) {
    uint8_t sdata[MD_SND_CMD_NKRO_LEN + 2] = {0x00};

    sdata[0] = MD_SND_CMD_SEND_NKRO;
    memcpy(&sdata[1], data, sizeof(sdata) - 2);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    md_report_submit(sdata, sizeof(sdata));
}

void md_send_consumer(uint8_t *data) {
    uint8_t sdata[MD_SND_CMD_CONSUMER_LEN + 2] = {0x00};

    sdata[0] = MD_SND_CMD_SEND_CONSUMER;
    memcpy(&sdata[1], data, sizeof(sdata) - 2);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    md_report_submit(sdata, sizeof(sdata));
}

void md_send_system(uint8_t *data) {
    uint8_t sdata[MD_SND_CMD_SYSTEM_LEN + 2] = {0x00};

    sdata[0] = MD_SND_CMD_SEND_SYSTEM;
    memcpy(&sdata[1], data, sizeof(sdata) - 2);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    md_report_submit(sdata, sizeof(sdata));
}

void md_send_fn(uint8_t *data) {
    uint8_t sdata[MD_SND_CMD_FN_LEN + 2] = {0x00};

    sdata[0] = MD_SND_CMD_SEND_FN;
    memcpy(&sdata[1], data, sizeof(sdata) - 2);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    md_report_submit(sdata, sizeof(sdata));
}

void md_send_mouse(uint8_t *data) {
    uint8_t sdata[MD_SND_CMD_MOUSE_LEN + 2] = {0x00};

    sdata[0] = MD_SND_CMD_SEND_MOUSE;
    memcpy(&sdata[1], data, sizeof(sdata) - 2);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    md_report_submit(sdata, sizeof(sdata));
}

void md_send_devinfo(const char *name) {
    uint8_t sdata[MD_SND_CMD_DEVINFO_LEN + 3] = {0x00};
    size_t  infolen                           = strlen(name);

    if (infolen > MD_SND_CMD_DEVINFO_LEN) {
        return;
    }

    sdata[0] = MD_SND_CMD_SEND_DEVINFO;
    sdata[1] = infolen;

    memcpy(&sdata[2], name, infolen);
    md_calc_check_sum(sdata, infolen + 2);
    if (!smsg_push(sdata, sizeof(sdata))) {
        drop_count++;
    }
}

void md_send_devctrl(uint8_t cmd) {
    uint8_t sdata[3] = {0x00};

    sdata[0] = MD_SND_CMD_DEVCTRL;
    memcpy(&sdata[1], &cmd, sizeof(sdata) - 2);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    if (!smsg_push(sdata, sizeof(sdata))) {
        drop_count++;
    }
}

void md_send_manufacturer(char *str, uint8_t len) {
    uint8_t sdata[MD_SND_CMD_MANUFACTURER_LEN + 3] = {0x00};

    if (len > MD_SND_CMD_MANUFACTURER_LEN) {
        return;
    }

    sdata[0] = MD_SND_CMD_MANUFACTURER;
    sdata[1] = len;
    memcpy(&sdata[2], str, len);
    md_calc_check_sum(sdata, len + 2);
    if (!smsg_push(sdata, len + 3)) {
        drop_count++;
    }
}

void md_send_product(char *str, uint8_t len) {
    uint8_t sdata[MD_SND_CMD_PRODUCT_LEN + 3] = {0x00};

    if (len > MD_SND_CMD_PRODUCT_LEN) {
        return;
    }

    sdata[0] = MD_SND_CMD_PRODUCT;
    sdata[1] = len;
    memcpy(&sdata[2], str, len);
    md_calc_check_sum(sdata, len + 2);
    if (!smsg_push(sdata, len + 3)) {
        drop_count++;
    }
}

void md_send_vpid(uint16_t vid, uint16_t pid) {
    uint8_t  sdata[4 + 2] = {0x00};
    uint32_t vpid;

    vpid = ((uint32_t)pid << 16) | vid;

    sdata[0] = MD_SND_CMD_VPID;
    memcpy(&sdata[1], &vpid, sizeof(vpid));
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    if (!smsg_push(sdata, sizeof(sdata))) {
        drop_count++;
    }
}

void md_send_raw(uint8_t *data, uint8_t length) {
    uint8_t sdata[MD_RAW_SIZE + 2] = {0x00};

    if (length != MD_RAW_SIZE) {
        return;
    }

    sdata[0] = MD_SND_CMD_RAW;
    memcpy(&sdata[1], data, length);
    md_calc_check_sum(sdata, sizeof(sdata) - 1);
    if (!smsg_push(sdata, sizeof(sdata))) {
        drop_count++;
    }
}

void md_devs_change(uint8_t devs, bool reset) __attribute__((weak));
void md_devs_change(uint8_t devs, bool reset) {
    switch (devs) {
        case DEVS_USB: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
        } break;
        case DEVS_2G4: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_2G4);
            if (reset) {
                if (md_get_version() < 48) {
                    md_send_manufacturer(MD_DONGLE_MANUFACTURER, strlen(MD_DONGLE_MANUFACTURER));
                    md_send_product(MD_DONGLE_PRODUCT, strlen(MD_DONGLE_PRODUCT));
                } else { // Add Unicode character support starting from v48.
                    md_send_manufacturer((char *)USBSTR(MD_DONGLE_MANUFACTURER), sizeof(USBSTR(MD_DONGLE_MANUFACTURER)));
                    md_send_product((char *)USBSTR(MD_DONGLE_PRODUCT), sizeof(USBSTR(MD_DONGLE_PRODUCT)));
                }
                md_send_vpid(VENDOR_ID, PRODUCT_ID);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_CLEAN);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT1: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT1);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_CLEAN);
                md_send_devinfo(MD_BT1_NAME);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT2: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT2);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_CLEAN);
                md_send_devinfo(MD_BT2_NAME);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT3: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT3);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_CLEAN);
                md_send_devinfo(MD_BT3_NAME);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT4: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT4);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_CLEAN);
                md_send_devinfo(MD_BT4_NAME);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        case DEVS_BT5: {
            md_send_devctrl(MD_SND_CMD_DEVCTRL_BT5);
            if (reset) {
                md_send_devctrl(MD_SND_CMD_DEVCTRL_CLEAN);
                md_send_devinfo(MD_BT5_NAME);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_PAIR);
            }
        } break;
        default:
            break;
    }
}

bool md_inquire_bat(void) {
    if (smsg_is_busy()) {
        return false;
    }

    md_send_devctrl(MD_SND_CMD_DEVCTRL_INQVOL);

    return true;
}
