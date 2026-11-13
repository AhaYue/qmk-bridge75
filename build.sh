#!/bin/sh
# Sync this repository into a QMK checkout and build there.
# This repository is the source of truth: edit here, not in the QMK checkout.
#
#   BRIDGE75_TAG=power ./build.sh            # QMK checkout defaults to ../qmk-bridge75-ansi-norgb
#   QMK_DIR=/path/to/qmk_firmware ./build.sh
#
# Firmware, checksums and logs go to the firmware/ folder next to the QMK checkout.
set -eu
repo=$(cd "$(dirname "$0")" && pwd)
qmk=$(cd "${QMK_DIR:-$repo/../qmk-bridge75-ansi-norgb}" && pwd)
base=33292390974c75362ca30873c482d052e0bd7a63

if [ "$(git -C "$qmk" rev-parse HEAD)" != "$base" ]; then
    printf 'warning: %s is not at the tested QMK base %s\n' "$qmk" "$base" >&2
fi

mkdir -p "$qmk/keyboards/shortcut" "$qmk/tools"
for dir in keyboards/shortcut/bridge75 keyboards/shortcut/wireless tools/bridge75; do
    # Compare contents and skip timestamps: only files that really changed are copied and listed.
    rsync -rlpc -i --delete "$repo/$dir/" "$qmk/$dir/"
done

exec sh "$qmk/tools/bridge75/build.sh"
