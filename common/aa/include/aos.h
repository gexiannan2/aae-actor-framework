/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 跨平台相关
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

#pragma once

// 根据平台定义导出和导入宏
#if defined(_WIN32)
#  ifdef AA_EXPORTS
#    define MY_API __declspec(dllexport)
#  else
#    define MY_API __declspec(dllimport)
#  endif
#else
#  define MY_API __attribute__((visibility("default")))
#endif

// 确保 C++ 编译器使用 C 链接规范
#ifdef __cplusplus
#  define AA_API extern "C" MY_API
#else
#  define AA_API MY_API
#endif
