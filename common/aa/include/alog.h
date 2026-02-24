/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 日志模块【先于盘古开天辟地而存在，后于世界末日而消失】
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

#pragma once

#include <string>
#include <format>
#include <utility>
#include <cstdio>
#include <cstring>

#include "aos.h"

AA_API void* alog_addfile(const char* file_name);
AA_API void alog_print(void* file, int level, const char* txt_addr, std::size_t txt_size);
AA_API std::size_t alog_stack_trace_to_buf(char* buf, std::size_t buf_size, int start = 0);
AA_API void alog_setroot(const char* root, const char* process_flag);
AA_API void alog_setoutdebug(bool v);
AA_API void alog_print_screen(bool v);
AA_API void alog_set_print_sys_log_to_screen(bool v);
AA_API void alog_print_stack(bool v);
AA_API bool alog_is_print_debug();
AA_API bool alog_is_print_stack_on_error();
AA_API int alog_get_error_count();
AA_API void alog_get_buf(char** pbuf, std::size_t* psize);

namespace alog
{
    // 日志级别枚举
    enum LogLevel
    {
        LEVEL_INFO = 0,
        LEVEL_DEBUG,
        LEVEL_WARNING,
        LEVEL_ERROR
    };

    namespace pvt
    {
        /* ---------- 1. 把单个参数转成字符串（栈缓存） ---------- */
        inline void to_str(char* buf, std::size_t n, const std::string_view& v)           { std::snprintf(buf, n, "%s", v.data()); }
        inline void to_str(char* buf, std::size_t n, int v)           { std::snprintf(buf, n, "%d", v); }
        inline void to_str(char* buf, std::size_t n, unsigned int v) { std::snprintf(buf, n, "%u", v); }
        inline void to_str(char* buf, std::size_t n, long v)         { std::snprintf(buf, n, "%ld", v); }
        inline void to_str(char* buf, std::size_t n, unsigned long v){ std::snprintf(buf, n, "%lu", v); }
        inline void to_str(char* buf, std::size_t n, double v)       { std::snprintf(buf, n, "%.6f", v); }
        inline void to_str(char* buf, std::size_t n, const char* s)  { std::strncpy(buf, s, n - 1); buf[n - 1] = '\0'; }
        inline void to_str(char* buf, std::size_t n, const std::string& s) { to_str(buf, n, s.c_str()); }

        /* ---------- 2. 运行时解析 {} 并渲染 ---------- */
        template <typename... Args>
        std::size_t format_runtime(char* out, std::size_t outSize,
                                const char* fmt, Args&&... args)
        {
            /* 1. 默认构造数组（C++11 合法） */
            std::string pieces[sizeof...(Args)] = {};

            /* 2. 逐个转成字符串 */
            std::size_t idx = 0;
            auto fill = [&](auto&& v) {
                char tmp[256];
                to_str(tmp, sizeof(tmp), v);
                pieces[idx++] = tmp;
            };
            /* 用逗号表达式展开即可，C++11 可用 */
            using expander = int[];
            (void)expander{0, (fill(std::forward<Args>(args)), 0)...};

            /* 3. 按 fmt 拼结果 */
            std::string result;
            result.reserve(std::strlen(fmt) + sizeof...(Args) * 32);
            idx = 0;
            for (const char* p = fmt; *p; ++p) {
                if (*p == '{' && *(p + 1) == '}') {
                    if (idx < sizeof...(Args)) result += pieces[idx++];
                    ++p;                 // 跳过 '}'
                } else {
                    result += *p;
                }
            }

            /* 4. 输出到缓冲区 */
            std::size_t len = result.size();
            if (len >= outSize) len = outSize - 1;
            std::memcpy(out, result.data(), len);
            out[len] = '\0';
            return len;
        }

        template<typename... Args>
        inline void print(void* file, LogLevel level, const char* fmt, Args&&... args)
        {
            char* buf_addr;
            std::size_t buf_size;
            alog_get_buf(&buf_addr, &buf_size);

            std::size_t txt_size = format_runtime(buf_addr, buf_size, fmt,
                                                std::forward<Args>(args)...);
            if (level == LEVEL_ERROR && txt_size + 3 < buf_size && alog_is_print_stack_on_error())
            {
                buf_addr[txt_size++] = '\r';
                buf_addr[txt_size++] = '\n';
                std::size_t stack_size = alog_stack_trace_to_buf(buf_addr + txt_size, buf_size - txt_size, 1);
                txt_size += stack_size;
                buf_addr[txt_size] = 0;
            }
            alog_print(file, level, buf_addr, txt_size);
        }
    }    

    template<typename... Args>
    inline void info(const char* fmt, Args... args)
    {
        pvt::print(nullptr, LEVEL_INFO, fmt, args...);
    }

    template<typename... Args>
    inline void debug(const char* fmt, Args... args)
    {
        if (!alog_is_print_debug())
        {
            return;
        }
        pvt::print(nullptr, LEVEL_DEBUG, fmt, args...);
    }

    template<typename... Args>
    inline void warning(const char* fmt, Args... args)
    {
        pvt::print(nullptr, LEVEL_WARNING, fmt, args...);
    }

    template<typename... Args>
    inline void error(const char* fmt, Args... args)
    {
        pvt::print(nullptr, LEVEL_ERROR, fmt, args...);
    }

    class file final
    {
    public:
        template<typename... Args>
        inline void info(const char* fmt, Args... args)
        {
            pvt::print(this, LEVEL_INFO, fmt, args...);
        }

        template<typename... Args>
        inline void debug(const char* fmt, Args... args)
        {
            if (!alog_is_print_debug())
            {
                return;
            }
            pvt::print(this, LEVEL_DEBUG, fmt, args...);
        }

        template<typename... Args>
        inline void warning(const char* fmt, Args... args)
        {
            pvt::print(this, LEVEL_WARNING, fmt, args...);
        }

        template<typename... Args>
        inline void error(const char* fmt, Args... args)
        {
            pvt::print(this, LEVEL_ERROR, fmt, args...);
        }
    };

    inline file* addfile(const std::string& file_name)
    {
        return static_cast<file*>(alog_addfile(file_name.c_str()));
    }

    inline std::string stack_trace(int start = 0)
    {
        char* buf_addr;
        std::size_t buf_size;
        alog_get_buf(&buf_addr, &buf_size);
        std::size_t stack_size = alog_stack_trace_to_buf(buf_addr, buf_size, start);
        return std::string(buf_addr, stack_size);
    }

    inline void setroot(const char* root, const char* process_flag)
    {
        setroot(root, process_flag);
    }

    inline void setoutdebug(bool v)
    {
        alog_setoutdebug(v);
    }

    inline void print_screen(bool v)
    {
        alog_print_screen(v);
    }
    
    inline void set_print_sys_log_to_screen(bool v)
    {
        set_print_sys_log_to_screen(v);
    }
    
    inline void print_stack(bool v)
    {
        alog_print_stack(v);
    }
    
    inline int get_error_count()
    {
        return alog_get_error_count();
    }

}
