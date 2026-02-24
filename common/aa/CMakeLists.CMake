#
# 通用Linux C++ CMakeList
#
# ygluu, ai
#
# 2025-07-20 第2次改进
# 2025-04-19 第1次改进
# 2025-04-13 首版
#

# 公共CMakeLists（被项目CMakeLists include）

# **********************************************************************************
# 以下设置公共编译选项

# ┌──────────── 公共CMakeLists开始
message(STATUS ">>> ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓--${PROJECT_NAME}--的公共CMakeLists开始--↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓")

set(SRC_FILES "")

# ┌──────────── 强制使用 g++
cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_C_COMPILER   gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ┌──────────── Debug / Release 编译参数 + 提示
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)
if(CMAKE_BUILD_TYPE STREQUAL "debug")
    message(STATUS ">>> Build Mode: Debug (-g -O0)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -g -O0")
else()
    message(STATUS ">>> Build Mode: Release (-O3 -DNDEBUG)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -O3 -DNDEBUG")
endif()

list(TRANSFORM INC_DIRS PREPEND  "${CMAKE_CURRENT_SOURCE_DIR}/")
list(TRANSFORM LINK_DIRS PREPEND  "${CMAKE_CURRENT_SOURCE_DIR}/")

# ┌──────────── 绝对目录生成
message(STATUS ">>> 绝对目录生成......")
message(STATUS ">>> 项目目录 : ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS ">>> 相对OUT_DIR : ${OUT_DIR}")
message(STATUS ">>> 相对LIB_DIRS: ${LIB_DIRS}")
message(STATUS ">>> 相对SRC_DIRS: ${SRC_DIRS}")
#增加绝对路径
set(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${OUT_DIR})
list(TRANSFORM LIB_DIRS PREPEND  "${CMAKE_CURRENT_SOURCE_DIR}/")
list(TRANSFORM SRC_DIRS PREPEND  "${CMAKE_CURRENT_SOURCE_DIR}/")
if(PROJECT_TYPE STREQUAL "EXE")
    list(APPEND SRC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
    list(APPEND SRC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src")
else()
    list(APPEND SRC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
# 去除/../
get_filename_component(OUT_DIR "${OUT_DIR}" REALPATH)
set(CleanDirs "")
foreach(p IN LISTS LIB_DIRS)
    if(EXISTS "${p}")
        file(REAL_PATH "${p}" CleanDir BASE_DIRECTORY "/")
        list(APPEND CleanDirs "${CleanDir}")
    else()
        message(SEND_ERROR "库目录不存在: ${p}")
    endif()
endforeach()
set(LIB_DIRS "${CleanDirs}")
set(CleanDirs "")
foreach(p IN LISTS SRC_DIRS)
    if(EXISTS "${p}")
        file(REAL_PATH "${p}" CleanDir BASE_DIRECTORY "/")
        list(APPEND CleanDirs "${CleanDir}")
    else()
        message(SEND_ERROR "源目录不存在: ${p}")
    endif()
endforeach()
set(SRC_DIRS "${CleanDirs}")
# 去重
list(REMOVE_DUPLICATES LIB_DIRS)
list(REMOVE_DUPLICATES SRC_DIRS)
# 打印
message(STATUS ">>> 绝对OUT_DIR : ${OUT_DIR}")
message(STATUS ">>> 绝对LIB_DIRS: ${LIB_DIRS}")
message(STATUS ">>> 绝对SRC_DIRS: ${SRC_DIRS}")

# ┌──────────── 连接目录生成
message(STATUS ">>> 连接目录生成(LINK_DIRS)......")
set(LINK_DIRS ${LIB_DIRS})
list(TRANSFORM LINK_DIRS APPEND "/lib")
if(PROJECT_TYPE STREQUAL "EXE")
    list(APPEND LINK_DIRS "${OUT_DIR}")
endif()
list(REMOVE_DUPLICATES LINK_DIRS)
message(STATUS ">>> LINK_DIRS : ${LINK_DIRS}")

# ┌──────────── 连接名生成
message(STATUS ">>> 连接名生成(LINK_NAMES)......")
if(WIN32)
    set(STATIC_EXT .a)    # MinGW-w64 静态库扩展名（包括导入库）
    set(SHARED_EXT .dll)  # MinGW-w64 动态库扩展名
else()
    set(STATIC_EXT .a)    # Linux 静态库扩展名
    set(SHARED_EXT .so)   # Linux 动态库扩展名
endif()

# 查找库文件（优先查找导入库）
foreach(DIR ${LINK_DIRS})
    if(WIN32)
        # Windows: 优先查找导入库 (.dll.a) 和静态库 (.a)
        file(GLOB LIB_FILES 
             "${DIR}/*.dll.a"  # 导入库（优先）
             "${DIR}/*.a"      # 静态库
             "${DIR}/*.dll"    # DLL 文件（备用）
        )
    else()
        # Linux: 查找 .a 和 .so
        file(GLOB LIB_FILES "${DIR}/*.a" "${DIR}/*.so")
    endif()
    
    foreach(LIB_FILE ${LIB_FILES})
        get_filename_component(NAME ${LIB_FILE} NAME_WE)
        # 仅当 n 以 "lib" 开头时才去掉前 3 个字符
        if(NAME MATCHES "^lib(.+)")
            set(NAME "${CMAKE_MATCH_1}")
        endif()
        list(APPEND LINK_NAMES ${NAME})
    endforeach()
endforeach()
list(REMOVE_DUPLICATES LINK_NAMES)
message(STATUS ">>> LINK_NAMES: ${LINK_NAMES}")

# ┌──────────── 包含目录生成
message(STATUS ">>> 头文件目录生成(INC_DIRS)......")
set(INC_ROOTS "${LIB_DIRS}")
list(TRANSFORM INC_ROOTS APPEND "/include")
list(APPEND INC_ROOTS ${SRC_DIRS})
foreach(INC_ROOT ${INC_ROOTS})
    list(APPEND INC_DIRS ${INC_ROOT})
    file(GLOB_RECURSE SUBS LIST_DIRECTORIES true "${INC_ROOT}/*")
    foreach(SUB ${SUBS})
        if(IS_DIRECTORY ${SUB})
            list(APPEND INC_DIRS ${SUB})
        endif()
    endforeach()
endforeach()
message(STATUS ">>> INC_DIRS  : ${INC_DIRS}")

# ┌──────────── 源文件生成
message(STATUS ">>> 源文件生成(SRC_FILES)......")
foreach(DIR IN LISTS SRC_DIRS)
    file(GLOB_RECURSE FILES
         CONFIGURE_DEPENDS
         "${DIR}/*.c"
         "${DIR}/*.cc"
         "${DIR}/*.cpp"
         "${DIR}/*.cppm")
    list(APPEND SRC_FILES ${FILES})
endforeach()

# ┌──────────── 运行时 rpath：优先同级目录
if(NOT WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,$ORIGIN")
endif()

# ┌──────────── 设置头文件目录
message(STATUS ">>> 设置头文件目录......")
include_directories(${INC_DIRS})

# ┌──────────── 设置源文件
message(STATUS ">>> 设置源文件......")
if(PROJECT_TYPE STREQUAL "EXE")
    # 添加可执行文件
    add_executable(${PROJECT_NAME} ${SRC_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})
elseif(PROJECT_TYPE STREQUAL "DLL")
    # 添加动态链接库（SHARED）
    add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}  # .a/.dll.a (导入库/静态库)
        LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}  # .so (Linux 动态库)
        RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}  # .dll (Windows 动态库)
    )
elseif(PROJECT_TYPE STREQUAL "LIB")
    # 添加静态库（STATIC）
    add_library(${PROJECT_NAME} STATIC ${SRC_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}  # 设置.lib或.a的输出目录
    )
else()
    message(FATAL_ERROR "未知的项目类型: ${PROJECT_TYPE}")
endif()

# ┌──────────── 设置输出目录
message(STATUS ">>> 设置输出目录......")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})

# ┌──────────── 设置连接相关（LINK_DIRS、LINK_NAMES）
message(STATUS ">>> 设置连接相关（LINK_DIRS、LINK_NAMES）......")
if(WIN32)
    # Windows 平台
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--export-all-symbols")
    # 如果需要 Dbghelp.lib（用于堆栈跟踪等）
    target_link_libraries(${PROJECT_NAME} PUBLIC Dbghelp.lib)
else()
    # Linux / macOS / 其他 Unix
    target_link_options(${PROJECT_NAME} PUBLIC "-Wl,-E")
endif()
target_link_directories(${PROJECT_NAME} PUBLIC "${LINK_DIRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC "${LINK_NAMES}")

find_package(Threads REQUIRED)
if(TARGET ${PROJECT_NAME})
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

# ---------------------------- 复制最新 .so/.dll/.dll.a ----------------------------
# ---------------------------- 复制最新 .so ----------------------------
if(PROJECT_TYPE STREQUAL "EXE")
    set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/copy_so_final.stamp")

    # 1. 收集「所有可能产出 .so 的 target」
    #    包括：本项目 add_subdirectory 里的 SHARED 库 + 外部 IMPORTED
    set(SO_TARGETS "")
    # ① 本 CMakeLists 里已经定义的 SHARED 库（名字就是目录名）
    foreach(d ${LIB_DIRS})
        get_filename_component(target_name ${d} NAME)
        if(TARGET ${target_name})
            list(APPEND SO_TARGETS ${target_name})
        endif()
    endforeach()
    # ② 顶层可能还有别的 SHARED 目标，也加进来（可选）
    get_property(all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(t ${all_targets})
        get_target_property(type ${t} TYPE)
        if(type STREQUAL SHARED_LIBRARY)
            list(APPEND SO_TARGETS ${t})
        endif()
    endforeach()

    # 2. 复制命令：***真正依赖这些 SO_TARGETS***
    list(TRANSFORM LIB_DIRS APPEND "/lib")
    
    # 3. 收集需要复制的文件模式
    set(BUILD_SO_PATTERN "")
    foreach(t ${SO_TARGETS})
        list(APPEND BUILD_SO_PATTERN "$<TARGET_FILE:${t}>")
    endforeach()

    # 4. 创建跨平台复制脚本（替代 bash）
    set(COPY_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/copy_libs_${PROJECT_NAME}.cmake")
    file(WRITE "${COPY_SCRIPT}" "
        message(STATUS \">>> 复制库文件到 ${OUT_DIR}\")
        file(MAKE_DIRECTORY \"${OUT_DIR}\")
        
        # 复制构建树中的库
        foreach(lib ${BUILD_SO_PATTERN})
            if(EXISTS \"\${lib}\")
                message(STATUS \"  复制: \${lib}\")
                file(COPY \"\${lib}\" DESTINATION \"${OUT_DIR}\")
            endif()
        endforeach()
        
        # 复制 LIB_DIRS 中的第三方库（包括 .exe）
        foreach(dir ${LIB_DIRS})
            # 查找所有支持的库文件和可执行文件
            file(GLOB THIRD_PARTY_LIBS
                \"\${dir}/*.so\"
                \"\${dir}/*.dll\"
                \"\${dir}/*.dll.a\"
                \"\${dir}/*.a\"
                \"\${dir}/*.exe\"  # 添加 .exe 文件
            )
            foreach(lib \${THIRD_PARTY_LIBS})
                if(EXISTS \"\${lib}\")
                    message(STATUS \"  复制第三方文件: \${lib}\")
                    file(COPY \"\${lib}\" DESTINATION \"${OUT_DIR}\")
                endif()
            endforeach()
        endforeach()
        
        # 创建标记文件
        file(WRITE \"${STAMP_FILE}\" \"done\")
    ")

    add_custom_command(
        OUTPUT ${STAMP_FILE}
        COMMAND ${CMAKE_COMMAND} -P "${COPY_SCRIPT}"
        DEPENDS ${SO_TARGETS}
        COMMENT ">>> 复制最新 .so/.dll/.dll.a/.exe 完成"
        VERBATIM
    )

    add_custom_target(copy_so_to_bin_${PROJECT_NAME}_final
        DEPENDS ${STAMP_FILE}
    )
    add_dependencies(${PROJECT_NAME} copy_so_to_bin_${PROJECT_NAME}_final)
endif()

# ┌──────────── 公共CMakeLists结束
message(STATUS ">>> ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑--${PROJECT_NAME}的公共CMakeLists结束--↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑")