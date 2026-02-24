#
# 通用Linux C++ CMakeList
#
# ygluu, ai
#
# 2025-07-20 第2次改进
# 2025-04-19 第1次改进
# 2025-04-13 首版
#

# 顶层CMakeLists
# ┌──────────── 顶层CMakeLists
message(STATUS ">>> ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓--顶层CMakeLists开始--↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓")

# 最小版本要求
cmake_minimum_required(VERSION 4.0)
project(projs LANGUAGES C CXX)

# 一键自动发现所有源码（增删实时生效）
file(GLOB_RECURSE ALL_SRCS
     CONFIGURE_DEPENDS
     src/*.cpp
     src/*.c
     src/*.cc
     src/*.h
     src/*.hpp)

# ┌──────────── 在这里增加项目
add_subdirectory(common/aa/src/aa)
add_subdirectory(common/aa/src/aae)

# ┌──────────── 顶层CMakeLists
message(STATUS ">>> ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑--顶层CMakeLists结束--↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑")
