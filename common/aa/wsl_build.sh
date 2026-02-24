#!/usr/bin/env bash
# 用法：./wsl_build.sh [debug|release] [target|all]
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/build/wsl"
BUILD_TYPE=${1:-debug}
TARGET=${2:-all}

[[ -d $BUILD_DIR ]] || cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

if [[ "$TARGET" == "all" ]]; then
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j"$(nproc)"
else
    cmake --build "$BUILD_DIR" --target "$TARGET" --config "$BUILD_TYPE" -j"$(nproc)"
fi