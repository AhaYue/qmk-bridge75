# USB-only RAW HID for VIA. Wireless modes intentionally do not tunnel RAW HID.
# VIA_ENABLE itself lives in keymaps/default/rules.mk: QMK lint rejects VIA at
# the keyboard level.
BRIDGE75_USB_RAW_HID = yes

include keyboards/shortcut/wireless/wireless.mk

BATTERY_DRIVER_REQUIRED = yes
BATTERY_DRIVER = custom

# tools/bridge75/build.sh passes the tag and build time so Fn+D names the exact file.
ifneq ($(strip $(BRIDGE75_BUILD)),)
    OPT_DEFS += -DBRIDGE75_BUILD=\"$(strip $(BRIDGE75_BUILD))\"
endif
