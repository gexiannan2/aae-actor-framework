/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 每个CPP文件首行必须引用同文件用于日志输出和SO/DLL的API实体函数输出
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

#pragma once

#define AA_EXPORTS
#include <iostream>

#include <iostream>
#include <string_view>

// 编译期提取文件名（不含路径）
constexpr std::string_view extract_filename(const char* path) {
    const char* filename = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }
    return std::string_view(filename);
}

// 自动打印文件名，使用 __COUNTER__ 保证跨编译单元唯一性
#define MODULE_INIT() \
    namespace { \
        struct auto_init_##__COUNTER__ { \
            auto_init_##__COUNTER__() { \
                constexpr auto name = extract_filename(__FILE__); \
                std::cerr << ">>> Initing: " << name << std::endl; \
            } \
        } auto_init_instance_##__COUNTER__; \
    }

MODULE_INIT();
