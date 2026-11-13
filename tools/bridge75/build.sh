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
qmk lint -kb shortcut/bridge75/ansi_norgb > "$output/lint.log" 2>&1
sh keyboards/shortcut/wireless/tests/run.sh > "$output/test-wireless.log" 2>&1
python3 keyboards/shortcut/wireless/tests/lowpower/run.py > "$output/test-lowpower.log" 2>&1
python3 keyboards/shortcut/wireless/tests/board/run.py > "$output/test-board.log" 2>&1
qmk compile -kb shortcut/bridge75/ansi_norgb -km default -e WB32_WAKE_RESET_HACK=yes -e BRIDGE75_BUILD="${tag}-${stamp}" > "$output/build-reset.log" 2>&1
cp shortcut_bridge75_ansi_norgb_default.bin "$staging/${name}_resetwake.bin"
qmk compile -kb shortcut/bridge75/ansi_norgb -km default -e WB32_WAKE_RESET_HACK=no -e BRIDGE75_BUILD="${tag}-${stamp}" > "$output/build-soft.log" 2>&1
cp shortcut_bridge75_ansi_norgb_default.bin "$staging/$name.bin"
(cd "$staging" && shasum -a 256 "$name.bin" "${name}_resetwake.bin" > "$sums")
mv "$staging/$name.bin" "$staging/${name}_resetwake.bin" "$staging/$sums" "$output/"
printf 'Built %s.bin, %s_resetwake.bin and %s in %s\n' "$name" "$name" "$sums" "$output"
