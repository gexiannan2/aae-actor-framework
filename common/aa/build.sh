#!/bin/bash

set -e

# 设置构建目录
script_dir=$(dirname "$0")
build_dir="${script_dir}/build/projs"

if [ ! -d "$build_dir" ]; then
	mkdir -p "$build_dir"
fi

cd "${build_dir}" || exit

cmake "${script_dir}" -DCMAKE_BUILD_TYPE="$1"
if [ ! $? -eq 0 ]; then
	read -n 1 -s
	exit 1
fi

make
