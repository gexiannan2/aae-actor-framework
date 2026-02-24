#!/bin/bash

set -e

# 设置构建目录
script_dir=$(dirname "$0")
build_dir="${script_dir}/build"

if [ -d "$build_dir" ]; then
	rm -r "$build_dir"
fi

${script_dir}/build.sh "$1"

