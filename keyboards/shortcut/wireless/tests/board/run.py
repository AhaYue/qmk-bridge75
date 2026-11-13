#!/usr/bin/env python3
"""Run board initialization regressions against functions extracted from production C.

Only platform/transport boundaries are stubbed. No production function body is copied
into the tests; the function extractor fails if a required function disappears.
"""
from pathlib import Path
import os
import subprocess
import tempfile

here = Path(__file__).resolve().parent
wireless = here.parent.parent
root = wireless.parents[2]
source = (root / 'keyboards/shortcut/bridge75/bridge75.c').read_text()


def function(name):
    import re
    match = re.search(r'^(?:static )?(?:void|bool) ' + re.escape(name) + r'\([^\n]*\) \{', source, re.M)
    if match is None:
        raise RuntimeError(f'Production function missing: {name}')
    start = match.start()
    pos = source.index('{', start)
    depth = 1
    pos += 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos]

state = source[source.index('typedef union {'):source.index('uint8_t bat_level')]
state += source[source.index('static uint32_t indicator_frames;'):source.index('// Emit one character')]
names = ['eeconfig_init_kb', 'configure_status_leds', 'keyboard_post_init_kb', 'apply_module_sleep', 'md_recovery_kb', 'wireless_post_task', 'process_record_kb', 'wireless_devs_change_kb']
with tempfile.TemporaryDirectory(prefix='bridge75-board-') as directory:
    directory = Path(directory)
    unit = directory / 'board_test.c'
    unit.write_text((here / 'stub.h').read_text() + '\n' + state + '\n' + '\n\n'.join(map(function, names)) + '\n' + (here / 'test.c').read_text())
    binary = directory / 'board_test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-fsanitize=undefined', '-I', str(wireless), str(unit), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
