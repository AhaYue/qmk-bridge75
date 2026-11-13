#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
workspace=$(cd .. && pwd)
export PATH="$workspace/.venv-qmk/bin:/opt/homebrew/opt/arm-none-eabi-gcc@8/bin:/opt/homebrew/opt/arm-none-eabi-binutils/bin:/opt/homebrew/opt/make/libexec/gnubin:$PATH"
output="$workspace/firmware"
tag=${BRIDGE75_TAG:-final}
mkdir -p "$output"
staging=$(mktemp -d "$output/.bridge75-build.XXXXXX")
trap 'rm -rf "$staging"' EXIT
name=bridge75_ansi_norgb_${tag}
qmk lint -kb shortcut/bridge75/ansi_norgb > "$output/lint.log" 2>&1
sh keyboards/shortcut/wireless/tests/run.sh > "$output/test-wireless.log" 2>&1
python3 keyboards/shortcut/wireless/tests/lowpower/run.py > "$output/test-lowpower.log" 2>&1
python3 keyboards/shortcut/wireless/tests/board/run.py > "$output/test-board.log" 2>&1
qmk compile -kb shortcut/bridge75/ansi_norgb -km default -e WB32_WAKE_RESET_HACK=yes > "$output/build-reset.log" 2>&1
cp shortcut_bridge75_ansi_norgb_default.bin "$staging/${name}_resetwake.bin"
qmk compile -kb shortcut/bridge75/ansi_norgb -km default -e WB32_WAKE_RESET_HACK=no > "$output/build-soft.log" 2>&1
cp shortcut_bridge75_ansi_norgb_default.bin "$staging/$name.bin"
(cd "$staging" && shasum -a 256 "$name.bin" "${name}_resetwake.bin" > "SHA256SUMS-$tag")
mv "$staging/$name.bin" "$staging/${name}_resetwake.bin" "$staging/SHA256SUMS-$tag" "$output/"
printf 'Built both Bridge75 variants in %s\n' "$output"
