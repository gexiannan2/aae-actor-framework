#!/bin/sh
# 拿到当前脚本所在目录的绝对路径
RUN_DIR=$(cd "$(dirname "$0")" && pwd)

# 拼接可执行文件和入口 lua 文件路径
EXE="$RUN_DIR/../bin/aae"   # 若在 Linux 下把 .exe 去掉
ENTRY="$RUN_DIR/main.lua"

# 启动程序，后续参数原样传递
"$EXE" mainfile "$ENTRY" "$@"
