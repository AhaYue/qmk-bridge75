# Bridge75 ANSI NoRGB

QMK support for the Shortcut Studio Bridge75 ANSI NoRGB: WB32FQ95 MCU with a
proprietary CH582F wireless module (UART, 115200 baud). USB, Bluetooth 1–3 and
the 2.4 GHz receiver are supported. VIA works over USB only.

* Keyboard Maintainer: see the repository `AhaYue/qmk-bridge75`
* Hardware Supported: Bridge75 ANSI NoRGB (six WS2812 status LEDs: Caps Lock,
  Escape/battery, Bluetooth 1–3, 2.4 GHz)
* USB ID: `FFFE:003E`, bootloader `wb32-dfu`

Make example for this keyboard (after setting up your build environment):

    qmk compile -kb shortcut/bridge75/ansi_norgb -km default

Release builds are made with `build.sh` in the repository root, which runs lint
and the host tests, stamps a build tag into the firmware and also produces the
`_resetwake` fallback (`WB32_WAKE_RESET_HACK=yes`: the MCU restarts after STOP
wake and may lose the first keystroke).

## Bootloader

Hold **Esc** while plugging in USB, then flash with QMK Toolbox. Unplug and
replug afterwards so the keyboard leaves DFU.

## Keys (Fn layer, identical on the Windows and Mac layouts)

| Keys | Action |
| --- | --- |
| Fn+1 / 2 / 3 | Bluetooth host 1 / 2 / 3; hold 1 s to pair |
| Fn+4 | 2.4 GHz receiver; hold 1 s to pair |
| Fn+5 | USB |
| Fn+` | Toggle the module's 30-minute sleep (saved, default off) |
| Fn+D | Type a diagnostic line |
| Fn+B | Type the battery level (`bat=NN`) |
| Fn+Win (2nd key of the bottom row) | Switch Windows / Mac base layer (saved) |
| Fn+Esc | Reset EEPROM: default keymap, USB mode, module sleep off |
| Fn+F1…F12 | Brightness, Mission Control, Launchpad, F5, F6, media, mute, volume |

Diagnostic and battery output carries no trailing newline. It starts only when
all keys are released and the keyboard is on USB or connected wirelessly. The
line begins with `bridge75-<tag>-<YYYYMMDD-HHMM>` (`bridge75-dev` when built
without `build.sh`).

## Behaviour

* The MCU enters STOP after 5 minutes without input in wireless mode (never
  while a cable is attached) and wakes on a key press or module message,
  keeping the first keystroke.
* The saved transport is restored on the first housekeeping pass, because
  `protocol_post_init()` installs the USB driver after `keyboard_post_init_kb()`.
* Wireless mode stops and disconnects USB, so VIA is unavailable there. The VIA
  definition is `docs/via/bridge75-norgb-via.json` in the repository.
* Status LEDs ignore stored RGB settings. Invalid default layers reset to
  Windows; an EEPROM record with an unknown version resets to USB defaults.

Documentation (Chinese): `README.md` and `docs/` in the repository root.
