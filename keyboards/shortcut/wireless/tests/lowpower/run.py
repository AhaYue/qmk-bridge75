#!/usr/bin/env python3
"""Compile production lowpower/USB state machines with deterministic host stubs."""
from pathlib import Path
import os
import subprocess
import tempfile

here = Path(__file__).resolve().parent
wireless = here.parent.parent
cc = os.environ.get("CC", "cc")
with tempfile.TemporaryDirectory(prefix="bridge75-lowpower-") as directory:
    out = Path(directory)
    source = (wireless / "lowpower.c").read_text()
    source = "\n".join(line for line in source.splitlines() if not line.startswith("#include "))
    (out / "lowpower.c").write_text('#include "stub.h"\n' + source)
    transport = (wireless / "transport.c").read_text()
    usb = transport[transport.index("void usb_remote_wakeup(void) {"):]
    (out / "usb.c").write_text((here / "usb_prefix.h").read_text() + usb + (here / "usb_test.c").read_text())
    common = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(here), "-I", str(wireless)]
    for name, sources in [("lowpower", [out / "lowpower.c", here / "test.c"]), ("usb", [out / "usb.c"])]:
        binary = out / name
        subprocess.run(common + [str(path) for path in sources] + ["-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
