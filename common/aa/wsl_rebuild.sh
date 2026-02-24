#!/usr/bin/env bash
ROOT="$(cd "$(dirname "$0")" && pwd)"
rm -rf "$ROOT/build"
exec "$ROOT/wsl_build.sh" "${1:-debug}" "${2:-all}"