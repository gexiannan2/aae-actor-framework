#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROTO_DIR="$ROOT/snetpb/proto"
GEN_DIR="$ROOT/snetpb/script/gen"
PROTO_FILE="$PROTO_DIR/demo.proto"
OUT_FILE="$GEN_DIR/demo_pb.lua"

mkdir -p "$GEN_DIR"

if command -v protoc >/dev/null 2>&1; then
    protoc \
        --proto_path="$PROTO_DIR" \
        --descriptor_set_out="$GEN_DIR/demo.pb.desc" \
        "$PROTO_FILE"
else
    echo "[WARN] protoc not found, skip schema validation and descriptor output" >&2
fi

python3 "$ROOT/tools/proto_to_lua.py" "$PROTO_FILE" "$OUT_FILE"

echo "generated: $OUT_FILE"
