/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 时间功能函数单元
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

#include "tscns.h"

namespace atime
{
    // 线程安全的本地时间函数
    inline void get_localtime(tm* tm_buf, const time_t* time)
    {
    #ifdef _WIN64
        localtime_s(tm_buf, time);
    #else
        *tm_buf = *localtime(time);
    #endif
    }
    
    class my_tscns : public TSCNS
    {
    public:
        my_tscns()
        {
            init();
        }
    };

    inline static my_tscns tscns;
    
    inline std::uint64_t nsec()
    {
        return (std::uint64_t)tscns.rdns();
    }

    inline std::uint64_t usec()
    {
        return nsec() / 1000;
    }

    inline std::uint64_t msec()
    {
        return nsec() / 1000000;
    }

    inline std::uint64_t sec()
    {
        return nsec() / 1000000000;
    }
}
