#include "ahcpp.h"

#include <string>
#include <vector>
#include <format>
#include <iostream>

#include "atime.h"
#include "afile.h"
#include "astr.h"

#include "athd.h"
#include "a.log.h"

namespace alog
{
    mdata* get_mdata()
    {
        static mdata md_;   // 首次调用时构造，顺序可预测
        return &md_;
    }
    auto _ = get_mdata();

    log_file::log_file(const std::string& file_name)
    {
        file_name_ = file_name;
        file_name_upper_ = astr::upper(file_name);
    }

    log_file::~log_file()
    {
        if(fp_)
        {
            std::fclose(fp_);
            fp_ = nullptr;
        }
    }

    static std::string level_names[4] = {"info", "debug", "warning", "error"};
    static std::string level_names_upper[4] = {"INF", "DBG", "WAR", "ERR"};

    void log_file::rotate_file(struct tm* tm_info, int level, const std::string logdir, const std::string& process_flag_file_name)
    {
        if(fp_)
        {
            std::fclose(fp_);
        }
        hour_ = tm_info->tm_hour;

        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d-%H", tm_info);

        full_filename_ = logdir + "/" + std::string(time_str) + "_" + process_flag_file_name + "_" + file_name_
        + "_" + level_names[level] + ".log";

        fp_ = std::fopen(full_filename_.c_str(), "ab");
        if (!fp_)
        {
            std::cout << "日志文件打开失败：" << full_filename_ << std::endl;
        }
    }

    inline struct tm* safe_localtime(const time_t* t, struct tm* buf)
    {
    #ifdef _WIN32
        // Windows: localtime_s 返回 errno_t，参数顺序相反
        if (localtime_s(buf, t) == 0)
        {
            return buf;
        }
        return nullptr;  // 失败
    #else
        // Linux/POSIX: localtime_r
        return localtime_r(t, buf);
    #endif
    }
    
    void log_file::write(log_item* item, int pending_count, bool out_screen, bool print_sys_to_screen,
        const std::string logdir, const std::string& process_flag_file_name)
    {
        time_t time_sec = item->time / 1000;
        struct tm tm_info;
        safe_localtime(&time_sec, &tm_info);

        if (last_level_ != item->level || hour_ != tm_info.tm_hour || !fp_)
        {
            hour_ = tm_info.tm_hour;
            last_level_ = item->level;
            rotate_file(&tm_info, item->level, logdir, process_flag_file_name);
        }

        auto& buf = get_mdata()->buf_;        // std::array<char,2048>
        char* begin = buf.data();
        char* end   = begin + buf.size();

        // 使用 format_to_n，防止溢出；返回实际写到的尾后指针
        auto result = std::format_to_n(
            begin, buf.size(),
            "[{:02d}] [{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}] [{}-{}] [{}] >> ",
            pending_count==0?0:pending_count-1,
            tm_info.tm_year + 1900,
            tm_info.tm_mon  + 1,
            tm_info.tm_mday,
            tm_info.tm_hour,
            tm_info.tm_min,
            tm_info.tm_sec,
            static_cast<int>(item->time % 1000),
            file_name_upper_,
            level_names_upper[item->level],
            athd::gettname(item->tid)
        );

        std::size_t header_len = result.size;
        std::size_t total_len  = header_len + item->size;
        
        if (total_len <= buf.size() - 1)
        {
            // 如果 header + context 能放进缓冲区，则合并一次写
            std::memcpy(begin + header_len, item->context, item->size);
            begin[total_len] = 0;
            if (fp_)
            {
                std::fwrite(begin, 1, total_len, fp_);
            }
            if (!fp_ || (out_screen && (!is_sys_ || print_sys_to_screen)))
            {
                std::cout << begin;
            }
        }
        else
        {
            // 放不下就分两次写
            begin[header_len] = 0;
            if (fp_)
            {
                std::fwrite(begin, 1, header_len, fp_);
                std::fwrite(item->context, 1, item->size, fp_);
            }
            if (!fp_ || (out_screen && (!is_sys_ || print_sys_to_screen)))
            {
                std::cout << begin << item->context;
            }
        }
        if (fp_)
        {
            std::fflush(fp_);
            #ifdef _WIN32
                ::_commit(_fileno(fp_));  // Windows 立即落盘
            #else
                ::fsync(fileno(fp_));     // POSIX 立即落盘
            #endif
        }
    }
    
    mdata::mdata()
    {
        is_runing_ = true;
        work_thread_ = std::thread(&mdata::process_logs, this);
        log_files_.emplace_back(new log_file("sys"));
        log_files_[0]->is_sys_ = true;
    }

    mdata::~mdata()
    {
        stop();
    }

    inline void mdata::stop()
    {
        if (!is_runing_)
        {
            return;
        }
        is_runing_ = false;

        {
            auto item = static_cast<log_item*>(malloc(sizeof(log_item)));
            item->next = nullptr;
            item->time = 0;
            std::lock_guard<std::mutex> lock(mtx_);            
            if (head_) 
            {
                tail_->next = item;
                tail_ = item;
            }
            else
            {
                head_ = item;
                tail_ = item;
            }
            cv_.notify_one();
        }

        if (work_thread_.joinable())
        {
            work_thread_.join();
        }
        while(head_)
        {
            auto temp = head_;
            head_ = head_->next;
            delete temp;
        }
        head_ = nullptr;
    }

    inline std::tuple<log_item*, log_item*, int> mdata::get_items()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, 
            [this]
            {
                return (head_ != nullptr && is_settings_) || !is_runing_;
            }
        );

        log_item* h = (log_item*)head_;
        log_item* t = (log_item*)tail_;
        head_ = nullptr;
        tail_ = nullptr;

        return {h, t, item_count_};
    }

    inline void mdata::process_item(log_item* item)
    {
        log_file* file = nullptr;
        if (item->file)
        {
            file = static_cast<log_file*>(item->file);
        }
        else
        {
            file = log_files_[0].get();
        }
        try
        {
            file->write(item, item_count_, print_screen_, print_sys_log_to_screen_, root_dir_, process_flag_file_name_);
        }
        catch(const std::exception& e)
        {
            std::cerr << "写日志错误：" << e.what() << '\n' << item->context << std::endl;
        }
        catch(...)
        {
            std::cerr << "写日志错误: " << '\n' << item->context << std::endl;
        }
    }

    // thread_signal_mask.cpp
    #include <signal.h>
    #include <pthread.h>

    // 为当前线程屏蔽所有「会导致进程终止」的信号
    inline void block_all_signals()
    {
    #ifndef _WIN32
        sigset_t full;
        sigfillset(&full);
        pthread_sigmask(SIG_BLOCK, &full, nullptr);
    #endif
    }

    inline void mdata::process_logs()
    {
        block_all_signals();
        
        std::cout << "日志线程已启动，等待setroot..." << std::endl;

        auto is_stop = false;
        while (!is_stop || head_)
        {
            auto [head, tail, count] = get_items();

            auto curr = head;
            while(curr)
            {
                if (is_stop || !curr->time)
                {
                    is_stop = true;
                }
                else
                {
                    process_item(curr);
                }
                item_count_--;
                auto next = curr->next;
                free(curr);
                curr = next;
            }
        }

        std::cout << "日志线程已结束" << std::endl;
    }
    
}

AA_API void alog_setroot(const char* root, const char* process_flag)
{
    auto process_flag_file_name = std::string(process_flag);
    replace(process_flag_file_name.begin(), process_flag_file_name.end(), ':', '.');
    replace(process_flag_file_name.begin(), process_flag_file_name.end(), '[', '-');
    replace(process_flag_file_name.begin(), process_flag_file_name.end(), ']', '-');
    replace(process_flag_file_name.begin(), process_flag_file_name.end(), '(', '-');
    replace(process_flag_file_name.begin(), process_flag_file_name.end(), ')', '-');
    
    auto dir = std::string(root);
    if (dir.empty())
    {
        dir = afile::exe_path + "/log";
    }
    dir = afile::normalizepath(dir + "/" + process_flag);
    afile::mkdir(dir);

    auto md = alog::get_mdata();
    md->process_flag_ = process_flag;
    md->process_flag_file_name_ = process_flag_file_name;
    md->root_dir_ = dir;

    md->is_settings_ = true;
    md->cv_.notify_one();
}

AA_API void alog_setoutdebug(bool v)
{
    alog::get_mdata()->print_debug_ = v;
}

AA_API void alog_print_screen(bool v)
{
    alog::get_mdata()->print_screen_ = v;
}

AA_API void alog_set_print_sys_log_to_screen(bool v)
{
    alog::get_mdata()->print_sys_log_to_screen_ = v;
}

AA_API void alog_print_stack(bool v)
{
    alog::get_mdata()->print_stack_on_error = v;
}

AA_API bool alog_is_print_debug()
{
    return alog::get_mdata()->print_debug_;
}

AA_API bool alog_is_print_stack_on_error()
{
    return alog::get_mdata()->print_stack_on_error;
}

AA_API int alog_get_error_count()
{
    return alog::get_mdata()->error_count_;
}

AA_API void alog_get_buf(char** pbuf, std::size_t* psize)
{
    constexpr std::size_t max_size = 1 * 1024 * 1024;
    static thread_local std::vector<char> buf_(max_size + 1);
    *pbuf  = buf_.data();
    *psize = max_size;
}

AA_API void* alog_addfile(const char* file_name)
{
    auto md = alog::get_mdata();

    std::lock_guard<std::mutex> lock(md->mtx_);
    auto ret = new alog::log_file(file_name);
    md->log_files_.emplace_back(ret);

    return ret;
}

AA_API void alog_print(void* file, int level, const char* txt_addr, std::size_t txt_size)
{
    auto md = alog::get_mdata();
    if (!md->is_runing_)
    {
        return;
    }
    
    auto item = static_cast<alog::log_item*>(malloc(sizeof(alog::log_item) + txt_size + 3));
    
    item->next = nullptr;
    item->tid = athd::getctid();
    item->file = file;
    item->level = static_cast<alog::LogLevel>(level);
    item->time = atime::msec();
    auto buf = item->context;
    std::memcpy(buf, txt_addr, txt_size);
    auto idx = txt_size;
    buf[idx++] = '\r';
    buf[idx++] = '\n';
    buf[idx] = 0;
    item->size = idx;
    
    std::lock_guard<std::mutex> lock(md->mtx_);
    
    if (md->head_)
    {
        md->tail_->next = item;
        md->tail_ = item;
    }
    else
    {
        md->head_ = item;
        md->tail_ = item;
    }
    md->item_count_++;
    if (item->level == alog::LEVEL_WARNING)
    {
        md->warning_count_++;
    }
    else if (item->level == alog::LEVEL_ERROR)
    {
        md->error_count_++;
    }
    md->cv_.notify_one();

    // if (!md->is_settings_)
    // {
    //     std::cout << txt_addr << std::endl;
    // }
}
