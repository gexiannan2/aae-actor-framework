#
# 通用Linux C++ CMakeList
#
# ygluu, ai
#
# 2025-07-20 第2次改进
# 2025-04-19 第1次改进
# 2025-04-13 首版
#

# 仅需设置源码目录和库目录，其它的自动搜索

# 项目CMakeLists

cmake_minimum_required(VERSION 4.0.0)

# 自动读取父目录名为项目名
string(REGEX REPLACE ".*/(.*)" "\\1" PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR})
# 可修改项目名称
set(PROJECT_NAME "aae")
project(${PROJECT_NAME})

# ┌──────────── 项目CMakeLists开始
message(STATUS ">>> ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓--${PROJECT_NAME}--的CMakeLists开始↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓")

# ┌──────────── 在这里设置项目类型（可选值：EXE, DLL, LIB）
set(PROJECT_TYPE "EXE")

# ┌──────────── 在这里设置输出目录（相对目录， 默认*.so/*.dll放在这里，h文件在LIB_DIRS设置）
set(OUT_DIR "../../../../bin")

# ┌──────────── 在这里设置源码目录列表（相对目录，默认包含项目CMakeLists所在目录）
set(SRC_DIRS    
    
)

# ┌──────────── 在这里设置库目录列表（相对目录，默认lib_x下有子目录include、lib(*.a/*.lib)）
set(LIB_DIRS    
    "../lua"
    "../../"
)

# ┌──────────── 在这里设置安装在编译平台系统中的库等
set(INC_DIRS
    
)
set(LINK_NAMES

)
set(LINK_DIRS

)

# ┌──────────── 包含公共CMakeLists
include(../../CMakeLists.CMake)

# ┌──────────── 项目CMakeLists结束
message(STATUS ">>> ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑--${PROJECT_NAME}的CMakeLists结束--↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑")
