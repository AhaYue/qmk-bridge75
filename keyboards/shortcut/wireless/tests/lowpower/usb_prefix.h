#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "transport.h"
#define USB_SUSPENDED 2
#define USB_POWER_DOWN_DELAY 7000
#define USB_SUSPEND_WAKEUP_DELAY 0
struct {int state; unsigned status;} USB_DRIVER={0,2};
static uint32_t now=0;static unsigned downs=0,wakes=0;static bool key=false;
static transport_t transport = TRANSPORT_USB;
transport_t get_transport(void) { return transport; }
uint32_t sync_timer_read32(void){return now;}uint32_t sync_timer_elapsed32(uint32_t t){return now-t;}
void suspend_power_down(void){downs++;}bool suspend_wakeup_condition(void){return key;}void usbWakeupHost(void *p){(void)p;wakes++;}
