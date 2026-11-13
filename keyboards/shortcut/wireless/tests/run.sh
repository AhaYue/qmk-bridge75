#!/bin/sh
set -eu
cd "$(dirname "$0")"
binary=$(mktemp /tmp/bridge75-wireless-tests.XXXXXX)
trap 'rm -f "$binary"' EXIT
${CC:-cc} -std=c11 -g -Wall -Wextra -Wno-unused-parameter -Werror ${SANITIZERS:--fsanitize=undefined} -Istubs test_wireless.c -o "$binary"
"$binary"
