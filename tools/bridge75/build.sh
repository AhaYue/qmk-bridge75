#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
workspace=$(cd .. && pwd)
export PATH="$workspace/.venv-qmk/bin:/opt/homebrew/opt/arm-none-eabi-gcc@8/bin:/opt/homebrew/opt/arm-none-eabi-binutils/bin:/opt/homebrew/opt/make/libexec/gnubin:$PATH"
output="$workspace/firmware"
tag=${BRIDGE75_TAG:-final}
# Build time to the minute keeps successive builds of one tag apart.
stamp=$(date +%Y%m%d-%H%M)
mkdir -p "$output"
staging=$(mktemp -d "$output/.bridge75-build.XXXXXX")
trap 'rm -rf "$staging"' EXIT
name=bridge75_ansi_norgb_${tag}_${stamp}
sums=SHA256SUMS-${tag}-${stamp}
# Each build keeps its own logs so earlier builds are never overwritten.
logs="$output/logs-${tag}-${stamp}"
mkdir -p "$logs"
qmk lint -kb shortcut/bridge75/ansi_norgb > "$logs/lint.log" 2>&1
sh keyboards/shortcut/wireless/tests/run.sh > "$logs/test-wireless.log" 2>&1
python3 keyboards/shortcut/wireless/tests/lowpower/run.py > "$logs/test-lowpower.log" 2>&1
python3 keyboards/shortcut/wireless/tests/board/run.py > "$logs/test-board.log" 2>&1
qmk compile -kb shortcut/bridge75/ansi_norgb -km default -e WB32_WAKE_RESET_HACK=yes -e BRIDGE75_BUILD="${tag}-${stamp}" > "$logs/build-reset.log" 2>&1
cp shortcut_bridge75_ansi_norgb_default.bin "$staging/${name}_resetwake.bin"
qmk compile -kb shortcut/bridge75/ansi_norgb -km default -e WB32_WAKE_RESET_HACK=no -e BRIDGE75_BUILD="${tag}-${stamp}" > "$logs/build-soft.log" 2>&1
cp shortcut_bridge75_ansi_norgb_default.bin "$staging/$name.bin"
(cd "$staging" && shasum -a 256 "$name.bin" "${name}_resetwake.bin" > "$sums")
mv "$staging/$name.bin" "$staging/${name}_resetwake.bin" "$staging/$sums" "$output/"
printf 'Built %s.bin, %s_resetwake.bin and %s in %s (logs in %s)\n' "$name" "$name" "$sums" "$output" "$logs"
