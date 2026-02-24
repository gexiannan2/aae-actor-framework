/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 分布式ID模块
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

#pragma once

#include <cstdint>
#include <string>

#include "aos.h"

AA_API std::uint32_t aid_crc32(const char* data, std::size_t size);
AA_API std::uint64_t aid_gen();

namespace aid
{
    inline std::uint32_t crc32(const char* data, std::size_t size)
    {
        return aid_crc32(data, size);
    }

    inline std::uint32_t crc32(const std::string_view& s)
    {
        return aid_crc32(s.data(), s.size());
    }
    
    inline std::uint64_t gen()
    {
        return aid_gen();
    }
}
