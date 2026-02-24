/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 路径/文件功能函数单元
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

#pragma once

#include <algorithm>     // std::replace
#include <limits.h>      // PATH_MAX
#include <unistd.h>      // readlink
#ifdef _WIN64
#  include <windows.h>   // GetModuleFileNameA
#endif

#include <string>
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <cxxabi.h>
#include <vector>
#include <sstream>

#include "tscns.h"

namespace afile
{
    namespace pvt
    {
        inline const std::string& get_exe_path()
        {
            static std::string exe_path = []()
            {
                #ifdef _WIN64
                    // Windows 平台
                    char buffer[MAX_PATH];
                    GetModuleFileNameA(NULL, buffer, MAX_PATH);
                #else
                    // Linux 平台
                    char buffer[PATH_MAX];
                    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
                    if (len != -1) 
                    {
                        buffer[len] = '\0';
                    }
                #endif
                auto ret = std::string(buffer);
                std::replace(ret.begin(), ret.end(), '\\', '/');
                ret.erase(ret.find_last_of('/') + 1);   // <-- 去掉可执行文件名
                ret.pop_back();
                return ret;
            }();

            return exe_path;
        }
    }

    inline static std::string exe_path = pvt::get_exe_path();
    
    inline int mkdir(const std::string& dir)
    {
    #ifdef _WIN64
        return CreateDirectoryA(dir.c_str(), NULL);
    #else
        return std::filesystem::create_directories(dir);
    #endif
    }

    inline std::string normalizepath(const std::string& path) 
    {
        std::filesystem::path p(path);
        p = p.lexically_normal(); // 规范化路径
        return p.string();
    }

    inline std::string getfilename(const std::string& path)
    {
        std::filesystem::path p(path);
        return p.filename().string();
    }    

    inline void listsubdirs(const std::string& root,
                        std::vector<std::string>& result,
                        const std::string& exts,
                        bool recursive = true)
    {
        namespace fs = std::filesystem;
        if (!fs::exists(root) || !fs::is_directory(root))
            return;

        // 解析逗号分隔的扩展名（支持 ".cpp,.h" 或 "cpp,h" 格式）
        std::vector<std::string> extensions;
        if (!exts.empty())
        {
            std::istringstream iss(exts);
            std::string ext;
            while (std::getline(iss, ext, ','))
            {
                // 去除首尾空白
                ext.erase(0, ext.find_first_not_of(" \t"));
                ext.erase(ext.find_last_not_of(" \t") + 1);
                
                if (!ext.empty())
                {
                    // 确保扩展名以"."开头
                    if (ext[0] != '.') ext = "." + ext;
                    extensions.push_back(ext);
                }
            }
        }

        // 辅助函数：检查目录是否包含指定扩展名的文件
        auto has_required_files = [&extensions](const fs::path& dir_path) -> bool {
            if (extensions.empty()) return true;  // 无过滤条件，包含所有目录

            try {
                for (const auto& entry : fs::directory_iterator(dir_path))
                {
                    if (entry.is_regular_file() &&
                        std::find(extensions.begin(), extensions.end(),
                                entry.path().extension().string()) != extensions.end())
                        return true;
                }
            }
            catch (...) { /* 目录不可访问时跳过 */ }
            return false;
        };

        // 收集符合条件的目录
        if (recursive)
        {
            for (const auto& entry : fs::recursive_directory_iterator(root))
            {
                if (entry.is_directory() && has_required_files(entry.path()))
                    result.emplace_back(entry.path().generic_string());
            }
        }
        else
        {
            for (const auto& entry : fs::directory_iterator(root))
            {
                if (entry.is_directory() && has_required_files(entry.path()))
                    result.emplace_back(entry.path().generic_string());
            }
        }
    }  

    inline bool fileexists(const std::string& path)
    {
        namespace fs = std::filesystem;
        return fs::exists(path) && fs::is_regular_file(path);
    }

    inline bool direxists(const std::string& path)
    {
        namespace fs = std::filesystem;
        return fs::exists(path) && fs::is_directory(path);
    }
}
