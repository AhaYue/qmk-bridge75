#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define PRODUCT "Bridge75"
#define MANUFACTURER "Shortcut"
#define VENDOR_ID 0xfffe
#define PRODUCT_ID 0x003e
#define NKRO_ENABLE
#define KEYBOARD_REPORT_KEYS 6
#define NKRO_REPORT_BITS 30
#define REPORT_ID_SYSTEM 3
#define REPORT_ID_CONSUMER 4
typedef struct { uint8_t mods, reserved, keys[6]; } report_keyboard_t;
typedef struct { uint8_t mods, bits[NKRO_REPORT_BITS]; } report_nkro_t;
typedef struct { uint8_t buttons; int8_t x,y,v,h; } report_mouse_t;
typedef struct { uint8_t report_id; uint16_t usage; } report_extra_t;
typedef struct { uint8_t (*keyboard_leds)(void); void (*send_keyboard)(report_keyboard_t*); void (*send_nkro)(report_nkro_t*); void (*send_mouse)(report_mouse_t*); void (*send_extra)(report_extra_t*); } host_driver_t;
uint32_t sync_timer_read32(void);
uint32_t sync_timer_elapsed32(uint32_t time);
