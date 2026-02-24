#pragma once

#include <mutex>

#include "aargs.h"

namespace aargs
{
    class arger_impl
    {
    public:
        arger_impl()  = default;
        ~arger_impl() = default;

    public:
        std::unordered_map<std::string, int64_t>        i64_;
        std::unordered_map<std::string, bool>           b_;
        std::unordered_map<std::string, std::string>    s_;

        std::unordered_map<std::string, std::vector<int64_t>> vi64_;
        std::unordered_map<std::string, std::vector<bool>>    vb_;
        std::unordered_map<std::string, std::vector<std::string>> vs_;

        std::unordered_map<std::string, std::string> remark_;

        std::unordered_map<std::string, std::vector<int64_t*>>      vars_i64_;
        std::unordered_map<std::string, std::vector<bool*>>         vars_b_;
        std::unordered_map<std::string, std::vector<std::string*>>  vars_s_;
        std::unordered_map<std::string, std::vector<std::vector<int64_t>*>> vars_vi64_;
        std::unordered_map<std::string, std::vector<std::vector<bool>*>>    vars_vb_;
        std::unordered_map<std::string, std::vector<std::vector<std::string>*>> vars_vs_;

        /* 用于 get_XXX 返回的内部缓冲区，线程安全由调用方保证 */
        std::mutex buf_mtx_;
        std::vector<int64_t> buf_vi64_;
        std::vector<bool>    buf_vb_;
        std::vector<std::string> buf_vs_;   // 存储实际 string
        std::vector<const char*> buf_vstr_; // 存储指向 string 的指针

        std::vector<std::string> loaded_files_;
    };
    
}
