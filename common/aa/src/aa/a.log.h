#pragma once

#include <thread>
#include <cstring>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>
#include <condition_variable>

#include "alog.h"

namespace alog
{
    class log_item
    {
    public:
        log_item* next;                 // 单向链表指针
        std::uint64_t tid;
        void* file;                     // 日志标志
        LogLevel level;                 // 日志等级
        uint64_t time;                  // 日志时间
        int size;                       // 内容大小
        char context[0];                // 日志内容
    };

    class log_file
    {
    public:
        int hour_ = -1;
        LogLevel last_level_ = (LogLevel)-1;

        std::string file_name_;
        std::string file_name_upper_;
        std::string full_filename_;
        std::FILE* fp_ = nullptr;
        bool is_sys_ = false;
    public:
        log_file(const std::string& file_name);
        ~log_file();
        void rotate_file(struct tm* tm_info, int level, const std::string log_dir, const std::string& process_flag_file_name);        
        void write(log_item* item, int pending_count, bool out_screen, bool print_sys_to_screen, 
            const std::string log_dir, const std::string& process_flag_file_name);
    };

    class mdata
    {
    public:
        volatile log_item* head_ = nullptr;
        log_item* tail_ = nullptr;
        std::atomic_int item_count_ = 0;
        std::mutex mtx_;
        std::condition_variable cv_;
        std::thread work_thread_;
        
        std::vector<std::shared_ptr<log_file>> log_files_;
        std::string process_flag_;
        std::string process_flag_file_name_;
        std::string root_dir_;
        int error_count_ = 0;
        int warning_count_ = 0;
        bool print_debug_ = true;
        bool print_screen_ = false;
        bool print_sys_log_to_screen_ = false;
        bool print_stack_on_error = true;
        std::atomic_bool is_runing_ = false;
        std::atomic_bool is_settings_ = false;
        std::array<char, 1024*1024> buf_;
    public:
        mdata();
        ~mdata();
        inline void stop();
        inline std::tuple<log_item*, log_item*, int> get_items();
        inline void process_item(log_item* item);
        inline void process_logs();
    };
}
