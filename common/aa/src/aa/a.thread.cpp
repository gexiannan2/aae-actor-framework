#include "ahcpp.h"

#ifdef _WIN64
#  include <windows.h>   // GetCurrentThreadId
#else
#  include <unistd.h>   // syscall
#  include <sys/syscall.h> // SYS_gettid
#endif

#include <thread>
#include <iostream>

#include "aos.h"
#include "alog.h"
#include "a.thread.h"

namespace athd
{
    // 跨平台获取线程ID
    inline std::uint64_t os_curr_id()
    {
    #ifdef _WIN64
        return static_cast<uint64_t>(GetCurrentThreadId());
    #else
        return static_cast<std::uint64_t>(syscall(SYS_gettid));
    #endif
    }

    // 单例
    mdata* get_mdata()
    {
        static mdata md_;   // 首次调用时构造，顺序可预测
        return &md_;
    }    

    // 确保CPP初始化阶段被实例化    
    static thread_local thread_impl* curr_thread_ = nullptr;
    static athd::thread_impl* main_thread_ = nullptr;

    void check_new_main_thread()
    {
        if (!main_thread_)
        {
            main_thread_ = do_new_thread("tmain", 0);
            curr_thread_ = main_thread_;
            auto md = get_mdata();
            std::lock_guard<std::recursive_mutex> lk(md->mtx_);
            md->lua_threads_.push_back(static_cast<athd::thread_impl*>((void*)main_thread_));
        }        
    }

    // 完成本单元初始化
    static auto _ = []() -> bool
        {
            auto _ = get_mdata();
            check_new_main_thread();
            return true;
        }();

    #ifdef _WIN64
    std::atomic<bool> is_main_ready = false;
    void setmainready()
    {
        athd::is_main_ready = true;
    }
    #endif

    thread_impl::thread_impl(const char* name, int ms)
    {
        if (ms <= 50)
        {
            ms = 50;
        }
        thread_name_ = name;
        job_timeout_limit_ = ms;
    }

    thread_impl::~thread_impl()
    {
        if (work_thread_.joinable())
        {
            work_thread_.join();
        }
    }

    void thread_impl::wait_exec()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this]{ return is_exe_.load(); });
    }

    thread_local void* curr_job_restul_;
    thread_local bool curr_job_end_;
    
    void thread_impl::exec()
    {
        curr_thread_ = this;
        auto id = os_curr_id();
        auto md = get_mdata();
        md->thread_map_[id] = this;
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            is_exe_ = true;
        }
        cv_.notify_one(); 

        if (tfunc_)
        {
            tfunc_(tdata_, 1);
        }
        
        int count;
        node* h;
        node* t;
        while(true)
        {
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return job_head_ != nullptr || (!is_working_ && !job_head_); });
                h = job_head_;
                t = job_tail_;
                count = job_count_;
                job_head_ = nullptr;
                job_tail_ = nullptr;               
            }

            if (!h && !is_working_)
            {
                break;
            }

            auto curr = h;
            while (curr)
            {
                curr_job_name_ = curr->job_name_;

                if (curr->job_ptr_)
                {
                    if (curr->work_fn_)
                    {
                        curr_job_restul_ = nullptr;
                        curr->work_fn_(curr->job_ptr_);
                        curr->sender_->push_job(
                            (curr_job_name_ + "-result").c_str(),
                            curr->job_ptr_,
                            curr_job_restul_,
                            nullptr,
                            curr->done_fn_);
                        curr_job_restul_ = nullptr;
                    }
                    else
                    {
                        curr->job_ptr_->job_count_--;
                        curr_job_end_ = curr->job_ptr_->job_count_ == 0;
                        curr_job_restul_ = curr->result_;
                        curr->done_fn_(curr->job_ptr_);
                        curr_job_restul_ = nullptr;
                    }
                }
                else
                {
                    is_working_ = false;
                }
                job_count_--;
                curr_job_name_ = "";
                curr->job_name_ = "";
                curr = curr->next_;
            }

            if (h)
            {
                md->nodes_.frees(h, t, count);
            }
        }

        if (tfunc_)
        {
            tfunc_(tdata_, 2);
        }

        is_stop_ = true;
    }    

    void thread_impl::push_job(const char* job_name,
                athd::pvt::job* job_ptr,
                void* result,
                c_twork work_fn,
                c_tdone done_fn
            )
    {
        auto ok = false;

        auto md = get_mdata();
        auto node = md->nodes_.alloc();
        if (job_name)
        {
            node->job_name_ = job_name;
        }
        node->sender_ = curr_thread_;
        node->job_ptr_ = job_ptr;
        node->result_ = result;
        node->work_fn_ = work_fn;
        node->done_fn_ = done_fn;
        node->next_ = nullptr;

        mtx_.lock();
        ok = is_working_;
        if (ok)
        {
            if (job_head_)
            {
                job_tail_->next_ = node;
                job_tail_ = node;
            }
            else
            {
                job_head_ = node;
                job_tail_ = node;
            }
            job_count_++;
        }
        mtx_.unlock();

        if (ok)
        {
            cv_.notify_one();
        }
        else
        {
            done_fn(job_ptr);
        }
    }

    thread_impl* do_new_thread(const char* name, c_tfunc tfunc, void* tdata, int ms)
    {
        auto md = athd::get_mdata();
        if (!name || std::string(name).empty())
        {
            throw std::runtime_error("new_thread：请输入线程名");
        }

        auto t = std::make_unique<athd::thread_impl>(name, ms);
        t->tfunc_ = tfunc;
        t->tdata_ = tdata;

        thread_impl* ptr = t.get();

        std::lock_guard<std::recursive_mutex> lk(md->mtx_);
        md->threads_.push_back(std::move(t));
        md->pthreads_.push_back(ptr);

        return ptr;
    }
}

AA_API void  athd_pushpjob(void* pool,
                const char* job_name,
                std::uint64_t cid,
                void* job_ptr,
                c_twork work_fn,
                c_tdone done_fn)
{
    auto p = static_cast<athd::pool_impl*>(pool);
    if (!p)
    {
        throw std::runtime_error("push_job: 无效线程池指针");
    }

    std::uint64_t idx;
    if (cid)
    {
        idx = cid;
    }
    else
    {
        idx = (std::uint64_t)++p->index;
    }
    idx %= p->threads_.size();
    auto job = static_cast<athd::pvt::job*>(job_ptr);
    job->job_count_ = 1;
    p->threads_[idx]->push_job(job_name, job, nullptr, work_fn, done_fn);
}

AA_API void* athd_getresult(void)
{
    return athd::curr_job_restul_;
}

AA_API void athd_setresult(void* v)
{
    athd::curr_job_restul_ = v;
}

AA_API bool athd_jobend()
{
    return athd::curr_job_end_;
}

AA_API void* athd_allocjob(void)
{
    return static_cast<void*>(athd::get_mdata()->jobs_.alloc());
}

AA_API void  athd_pushtjob(void* t,
                const char* job_name,
                void* job_ptr,
                c_twork work_fn,
                c_tdone done_fn)
{
    if (t == (void*)1)
    {
        auto md = athd::get_mdata();
        std::lock_guard<std::recursive_mutex> lk(md->mtx_);
        auto& theads = md->lua_threads_;
        auto job = static_cast<athd::pvt::job*>(job_ptr);
        job->job_count_ = theads.size();
        for (auto& thread : theads)
        {
            thread->push_job(job_name, job, nullptr, work_fn, done_fn);
        }
        return;
    }

    if (t)
    {
        auto pt = static_cast<athd::thread_impl*>(t);
        if (!pt)
        {
            throw std::runtime_error("push_job: 无效线程指针");
        }
        auto job = static_cast<athd::pvt::job*>(job_ptr);
        job->job_count_ = 1;
        pt->push_job(job_name, job, nullptr, work_fn, done_fn);
        return;
    }
    
    auto md = athd::get_mdata();
    std::lock_guard<std::recursive_mutex> lk(md->mtx_);

    auto& theads = md->threads_;
    auto job = static_cast<athd::pvt::job*>(job_ptr);
    job->job_count_ = theads.size();
    for (auto& thread : theads)
    {
        thread->push_job(job_name, job, nullptr, work_fn, done_fn);
    }
}

AA_API void* athd_newthread(const char* name, c_tfunc tfunc, void* tdata, int ms)
{
    auto ret = athd::do_new_thread(name, tfunc, tdata, ms);
    ret->work_thread_ = std::thread(&athd::thread_impl::exec, ret);
    #ifdef _WIN64
    if (athd::is_main_ready)
    {
        ret->wait_exec();
    }
    #else
        ret->wait_exec();
    #endif
    return reinterpret_cast<void*>(ret);
}

AA_API void* athd_newpool(const char* name, std::size_t num, c_tfunc tfunc, void* tdata, int ms)
{
    auto md = athd::get_mdata();
    if (!name || std::string(name) == "")
    {
        throw std::runtime_error("new_conc_pool：请输入线程名");
    }
    if (!num)
    {
        throw std::runtime_error("new_conc_pool：线程数量不能为0");
    }

    std::lock_guard<std::recursive_mutex> lk(md->mtx_);

    auto pool = std::make_unique<athd::pool_impl>();
    for (int i = 0; i < num; ++i)
    {
        auto tptr = athd::do_new_thread((std::string(name) + "-" + std::to_string(i)).c_str(), tfunc, tdata, ms);
        tptr->work_thread_ = std::thread(&athd::thread_impl::exec, tptr);
        pool->threads_.push_back(std::move(std::unique_ptr<athd::thread_impl>(tptr)));
        md->pthreads_.push_back(tptr);
    }
    void* pptr = pool.get();
    md->pools_.push_back(std::move(pool));

    return reinterpret_cast<void*>(pptr);
}

AA_API void athd_setjobcapecity(std::size_t v)
{
    auto md = athd::get_mdata();
    md->jobs_.init(v);
}

AA_API void  athd_setjobtimeoutlimit(void* tp, std::size_t v)
{
    if (!tp)
    {
        tp = athd::curr_thread_;
    }
    auto t = static_cast<athd::thread_impl*>(tp);
    if (!t)
    {
        return;
    }
    t->job_timeout_limit_ = v;
}

AA_API void athd_freejob(void* job)
{
    athd::get_mdata()->jobs_.free(static_cast<athd::pvt::job*>(job));
}

AA_API void* athd_getmain()
{
    athd::check_new_main_thread();
    return athd::main_thread_;
}

AA_API void* athd_getct(void)
{
    athd::check_new_main_thread();
    return athd::curr_thread_;
}

AA_API std::uint64_t athd_getctid()
{
    static thread_local std::uint64_t id = athd::os_curr_id();
    return id;
}    

AA_API int athd_getpendingcount(void* pool, uint64_t cid)
{
    auto pts = static_cast<athd::pool_impl*>(pool);
    if (!pts)
    {
        throw std::runtime_error("get_pending_count: 无效线程池指针");
    }
    if (cid)
    {
        return pts->threads_[cid % pts->threads_.size()]->job_count_;
    }
    std::size_t ret = 0;
    for (auto& t : pts->threads_)
    {
        ret += t->job_count_;
    }
    return ret;
}

AA_API void athd_waitstops(void)
{
    auto md = athd::get_mdata();
    for (auto t : md->pthreads_)
    {
        t->stop();
    }
    for(;;)
    {
        auto stop_count = 0;
        for (auto t : md->pthreads_)
        {
            if (t->is_stop_)
            {
                stop_count++;
            }
        }
        if (stop_count == md->pthreads_.size())
        {
            return;
        }
        usleep(10);
    }    
}

AA_API const char* athd_gettname(std::uint64_t tid)
{
    auto md = athd::get_mdata();
    std::unique_lock<std::recursive_mutex> lk(md->mtx_);

    auto map = md->thread_map_;
    auto it = map.find(tid);
    if (it == map.end())
    {
        return "tmain";
    }
    return (const char*)it->second->thread_name_.data();
}

AA_API int athd_getpoolthreads(void* pool, void** threads, std::size_t* size)
{
    auto p = static_cast<athd::pool_impl*>(pool);
    if (*size < p->threads_.size())
    {
        return 0;
    }
    *size = p->threads_.size();
    for (auto i = 0; i < (int)*size; i++)
    {
        threads[i] = (void*)p->threads_[i].get();
    }
    return 1;
}
