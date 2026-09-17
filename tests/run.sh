#!/bin/bash
# Builds and runs the host-side tests for the config, profile, preset and
# sensor-selection layers. Needs a normal host gcc (not devkitPro): the real
# common/ sources are compiled against a small <switch.h> stand-in in shim/.
set -e

TESTS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="$( cd "$TESTS_DIR/.." && pwd )"
OUT_DIR="$TESTS_DIR/out"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

gcc -g -O0 -Wall \
    -I "$TESTS_DIR/shim" \
    -I "$ROOT_DIR/common/include" \
    -I "$ROOT_DIR/common/libs/minIni/include" \
    -I "$ROOT_DIR/common/libs/minIni/dev" \
    -I "$ROOT_DIR/common/libs/hocclk/include" \
    -x c++ -std=c++17 \
    "$TESTS_DIR/test_profiles.cpp" \
    "$ROOT_DIR/common/source/config.cpp" \
    "$ROOT_DIR/common/source/presets.cpp" \
    "$ROOT_DIR/common/source/sensor.cpp" \
    "$ROOT_DIR/common/source/fan.cpp" \
    -x c \
    "$ROOT_DIR/common/libs/minIni/dev/minIni.c" \
    -lstdc++ -lm -o "$OUT_DIR/test_profiles"

# The tests create config/ relative to the working directory, so run them in
# the scratch output folder rather than the repo root.
cd "$OUT_DIR"
./test_profiles
