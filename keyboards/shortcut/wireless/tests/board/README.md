# Board startup regressions

Run from any directory:

```sh
python3 keyboards/shortcut/wireless/tests/board/run.py
```

The runner reads the current `bridge75.c`, extracts its state and named production
functions, then compiles them with host stubs and UndefinedBehaviorSanitizer. It
never carries a copied implementation of the board functions.

Coverage: delayed startup after a simulated QMK `protocol_post_init`, device/name
queue ordering, one-time restoration, RGB renderer and LED power restoration,
persistent default-layer validation, and 100 ms configuration scheduling at timer
zero and across 32-bit wrap.

The transport boundary stub invokes the actual extracted device-change board
callback. This test checks board orchestration; it does not emulate QMK's complete
USB transport, WS2812 electrical timing, UART or the closed-source wireless module.
