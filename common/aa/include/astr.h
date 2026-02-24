/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 字符串功能函数单元
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

#pragma once

#include <string>
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <cxxabi.h>
#include <vector>
#include <cctype>
#include <string_view>
#include <cstring>

#include "tscns.h"

namespace astr
{
    template<typename... Args>
    inline std::string format(const char* fmt, Args... args)
    {
        return vformat(fmt, std::make_format_args(args...));
    }

    inline std::string uint64_to_hex(uint64_t value) 
    {
        std::stringstream oss;
        oss << "0x" << std::hex << value;
        return oss.str();
    }

    inline std::vector<std::string> split(const std::string& s, char delim = ' ')
    {
        std::vector<std::string> out;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim))
        {
            out.emplace_back(item);
        }
        return out;
    }    

    // 底层：已知长度，不依赖 '\0'
    inline std::string upper(const char* s, size_t len)
    {
        std::string out;
        if (!s || len == 0) return out;
        out.reserve(len);
        for (size_t i = 0; i < len; ++i)
        {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(s[i]))));
        }
        return out;
    }

    // 便利封装：C 风格字符串
    inline std::string upper(const char* s)
    {
        return upper(s, std::strlen(s ? s : ""));
    }

    // string_view 重载：直接拿长度，避免 c_str()
    inline std::string upper(std::string_view s)
    {
        return upper(s.data(), s.size());
    }
}
