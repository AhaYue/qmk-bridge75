#include "stub.h"
#include <assert.h>
#include <stdio.h>
struct usb_driver USB_DRIVER = {0};
static uint32_t now=300001, activity=0; static bool pending=false, busy=false; static unsigned downs=0, ups=0, recoveries=0;
static bool held;
uint16_t matrix_get_row(uint8_t row) { return row == 1 && held; }
uint32_t sync_timer_read32(void){return now;} uint32_t sync_timer_elapsed32(uint32_t t){return now-t;} uint32_t last_input_activity_elapsed(void){return now-activity;}
uint8_t wireless_get_current_devs(void){return 1;} bool smsg_is_busy(void){return busy;} bool md_reports_pending(void){return pending;} void md_recover(void){recoveries++;}
void suspend_power_down(void){downs++;} void suspend_wakeup_init_quantum(void){ups++;} void matrix_init_pins(void){} void last_matrix_activity_trigger(void){activity=now;}
void chSysLock(void){} void chSysUnlock(void){} void chSysDisable(void){} void chSysEnable(void){}
bool mcu_stop_mode(void){lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_UART);return true;}
int main(void){
 pending=true; lpwr_task();assert(lpwr_get_state()==LPWR_NORMAL);pending=false;
 held = true;
 lpwr_task();
 assert(lpwr_get_state() == LPWR_NORMAL && downs == 0);
 held = false;
 lpwr_task();assert(lpwr_get_state()==LPWR_PRESLEEP);now+=100;activity=now;lpwr_task();assert(lpwr_get_state()==LPWR_NORMAL&&downs==0);
 now+=300001;lpwr_task();now+=201;lpwr_task();assert(lpwr_get_state()==LPWR_STOP&&downs==1);busy=true;lpwr_task();assert(lpwr_get_state()==LPWR_NORMAL&&ups==1);busy=false;
 now+=300001;lpwr_task();now+=201;lpwr_task();now+=201;lpwr_task();assert(lpwr_get_state()==LPWR_WAKEUP&&recoveries==1&&lpwr_get_wakeup_count()==1);now+=201;lpwr_task();assert(lpwr_get_state()==LPWR_NORMAL&&ups==2);
 puts("PASS lowpower: held keys, pending reports, pre-sleep activity, STOP queue cancellation, UART resume, wake count");
}
