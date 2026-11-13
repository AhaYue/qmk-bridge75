#include <stdint.h>
#include <stdbool.h>
#include "lowpower.h"
#define DEVS_USB 0
#define USB_ACTIVE 1
#define MATRIX_ROWS 2
uint16_t matrix_get_row(uint8_t row);
struct usb_driver { int state; }; extern struct usb_driver USB_DRIVER;
uint32_t sync_timer_read32(void); uint32_t sync_timer_elapsed32(uint32_t t); uint32_t last_input_activity_elapsed(void);
uint8_t wireless_get_current_devs(void); bool smsg_is_busy(void); bool md_reports_pending(void); void md_recover(void);
void suspend_power_down(void); void suspend_wakeup_init_quantum(void); void chSysLock(void); void chSysUnlock(void); void chSysDisable(void); void chSysEnable(void);
