WIRELESS_ENABLE ?= yes
WIRELESS_DIR = $(TOP_DIR)/keyboards/shortcut/wireless

ifeq ($(strip $(WIRELESS_ENABLE)), yes)
    OPT_DEFS += -DWIRELESS_ENABLE -DNO_USB_STARTUP_CHECK

    # RAW HID is supported over USB only. The vendor shim that tunnels RAW HID
    # through the module is not maintained, so require the USB-only opt-in.
    ifeq ($(strip $(RAW_ENABLE)), yes)
        ifneq ($(strip $(BRIDGE75_USB_RAW_HID)), yes)
            $(error Bridge75 supports RAW HID over USB only; set BRIDGE75_USB_RAW_HID=yes)
        endif
    endif

    ifeq ($(strip $(BRIDGE75_USB_RAW_HID)), yes)
        OPT_DEFS += -DBRIDGE75_USB_RAW_HID
    endif

    ifeq ($(strip $(WB32_WAKE_RESET_HACK)), yes)
        OPT_DEFS += -DWB32_WAKE_RESET_HACK
    endif

    UART_DRIVER_REQUIRED ?= yes
    WIRELESS_LPWR_STOP_ENABLE ?= yes

    VPATH += $(WIRELESS_DIR)

    SRC += \
        $(WIRELESS_DIR)/wireless.c \
        $(WIRELESS_DIR)/transport.c \
        $(WIRELESS_DIR)/lowpower.c \
        $(WIRELESS_DIR)/smsg.c \
        $(WIRELESS_DIR)/module.c

    # The vendor shim only tunnels RAW HID through the module; wired VIA uses
    # QMK's standard raw HID directly.
    ifneq ($(strip $(BRIDGE75_USB_RAW_HID)), yes)
        SRC += $(WIRELESS_DIR)/md_raw.c
    endif

    ifeq ($(strip $(WIRELESS_LPWR_STOP_ENABLE)), yes)
        OPT_DEFS += -DWIRELESS_LPWR_STOP_ENABLE
        SRC += $(WIRELESS_DIR)/lpwr_wb32.c
    endif
endif
