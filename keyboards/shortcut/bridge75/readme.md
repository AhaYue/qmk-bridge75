# Bridge75 ANSI NoRGB wireless port

This target supports the Shortcut Studio Bridge75 ANSI NoRGB with a WB32FQ95
MCU and a proprietary CH582F wireless module. The six WS2812 status LEDs are
retained: Caps Lock, Escape battery, Bluetooth 1–3, and 2.4 GHz.

## Build and flash

```sh
sh tools/bridge75/build.sh
```

The script runs lint and the host-side regressions, then builds
`bridge75_ansi_norgb_final.bin` in the workspace's `firmware` directory.
Use this file for normal operation. The separate `_final_resetwake.bin` fallback
restarts the MCU after STOP wake and can lose the initial keystrokes.

Hold Escape while inserting the USB cable to enter the `wb32-dfu` bootloader.
Use QMK Toolbox to flash the supplied `.bin`; do not disconnect during flashing.
The USB identity is `FFFE:003E`; old VIA definitions are incompatible. VIA is
outside this port's supported scope.

## Default keys

The right key next to right Alt/GUI is Fn. Both Windows and Mac Fn layers have:

| Combination | Action |
| --- | --- |
| Fn + grave | USB |
| Fn + 1 / 2 / 3 | Bluetooth host 1 / 2 / 3 |
| Fn + 4 | 2.4 GHz |
| Hold Fn + 1 / 2 / 3 / 4 for one second | Pair the selected host |
| Fn + 5 | Toggle the wireless module's 30-minute sleep policy; saved in EEPROM |
| Fn + D | Type diagnostics after all keys and modifiers are released |
| Fn + left Alt/GUI position | Switch Windows/Mac base layer |
| Fn + Escape | Reset EEPROM and return to USB defaults |

Open a plain text editor before Fn+D. Keep the keyboard idle while the line is
typed. The line starts with `bridge75-final` and includes the module firmware byte (`fw`), selected device,
connection state, STOP wake count and source, transmit retries, dropped messages,
and module sleep setting (`sleep=0` means disabled). Output pauses while keys
are held or wireless is disconnected. Counters reset on MCU restart; the reset
fallback therefore cannot preserve pre-reset wake statistics.

A versioned keyboard EEPROM record defaults to USB and module sleep disabled.
An old record is reset on first boot. The module sleep commands exist in the
protocol, but their effect on this hardware must be measured. Disabling module
sleep can increase battery use. MCU STOP sleep remains enabled after five minutes
of inactivity; the status LED timeout is one minute.

## Hardware checks

Check USB, all Bluetooth hosts, 2.4 GHz, pairing, Caps and battery indicators.
After 6 minutes and again after at least 35 minutes of wireless idle, type a word
immediately and verify every letter. Capture diagnostics in each mode. Repeat the
35-minute check with module sleep enabled and monitor standby battery use. Compare
the reset fallback if normal wake fails.

The default base layers use standard QMK US keycodes. Diagnostic ASCII output
assumes the host uses a compatible keyboard layout.

## Startup and indicators

Saved wireless transport is restored on the first main-loop pass, after QMK's
`protocol_post_init()` installs its USB host driver. Device changes requeue module
name/configuration after flushing the previous device's TX queue. Control-only
transmit failures trigger reconnection without requiring a new keystroke.
Status LEDs are enabled independently of old RGB enable/mode EEPROM settings and
restored after a mode change. Fn shows blue Escape feedback before battery data is
available. Invalid persistent default layers are reset to the Windows base.
Verify basic Fn, pairing and USB return before repeating the long-idle tests.
