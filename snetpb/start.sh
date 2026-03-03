#!/bin/sh

RUN_DIR=$(cd "$(dirname "$0")" && pwd)
EXE="$RUN_DIR/../bin/aae"
ENTRY="$RUN_DIR/main.lua"

"$EXE" mainfile "$ENTRY" "$@"
