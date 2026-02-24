#include <pthread.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <string>

#include "athd.h"

namespace athd
{
    class thread_impl;

    struct node
    {
        node* next_;
        std::string job_name_;
        thread_impl* sender_;
        athd::pvt::job* job_ptr_;
        void* result_;
        c_twork work_fn_;
        c_tdone done_fn_;
    };

    class thread_impl
    {
    public:
        // 普通构造
        thread_impl(const char* name, int ms);
        // 移动构造 / 移动赋值：允许 vector 重新分配内存
        thread_impl(thread_impl&&) noexcept = default;
        thread_impl& operator=(thread_impl&&) noexcept = default;
        // 明确删除拷贝构造 / 拷贝赋值
        thread_impl(const thread_impl&)            = delete;
        thread_impl& operator=(const thread_impl&) = delete;
        ~thread_impl();

        void exec();
        void wait_exec();
        void push_job(const char* job_name,
                athd::pvt::job* job_ptr,
                void* result,
                c_twork work_fn,
                c_tdone done_fn);
        void stop()
        {
            push_job(nullptr, nullptr, nullptr, nullptr, nullptr);
        }

    public:
        std::thread               work_thread_;
        std::string               thread_name_;
        std::string               curr_job_name_;
        std::atomic_bool          is_exe_{false};
        std::atomic_bool          is_working_{true};
        std::atomic_bool          is_stop_{false};
        std::mutex                mtx_;
        std::condition_variable   cv_;
        std::atomic_char32_t      job_count_{0};
        node*                     job_head_ = nullptr;
        node*                     job_tail_ = nullptr;
        std::uint64_t             job_timeout_limit_ = 50;
        c_tfunc                   tfunc_;
        void*                     tdata_;
    };

    class pool_impl
    {   
    public:
        pool_impl() = default;
        pool_impl(pool_impl&&) noexcept = default;
        pool_impl& operator=(pool_impl&&) noexcept = default;
        pool_impl(const pool_impl&)            = delete;
        pool_impl& operator=(const pool_impl&) = delete;

    public:
        std::atomic_uint64_t index = 0;
        std::vector<std::unique_ptr<thread_impl>> threads_;
    };

    // ---------------------------- 对象池 ------------------------------------
    const std::size_t default_job_capecity = 1000000;

    template<typename Node>
    class list
    {
    public:
        void init(std::size_t capa)
        {
            free_count_ = capa;
            threads_.reset(new Node[capa]);
            build_free_list();
        }

        // 阻塞直到拿到节点，永不返回 nullptr
        Node* alloc()
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return free_ != nullptr; });

            Node* n = free_;
            free_ = free_->next_;
            --free_count_;

            return n;
        }

        // 带超时的版本
        Node* alloc_for(std::chrono::milliseconds timeout)
        {
            std::unique_lock<std::mutex> lk(mtx_);
            if (!cv_.wait_for(lk, timeout, [this] { return free_ != nullptr; }))
            {
                return nullptr;
            }
            
            Node* n = free_;
            free_ = free_->next;
            --free_count_;
            
            return n;
        }

        void free(Node* n)
        {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                n->next_ = free_;
                free_   = n;
                ++free_count_;
            }
            cv_.notify_one();
        }

        void frees(Node* h, Node* t, int count)
        {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                t->next_ = free_;
                free_   = h;
                free_count_ += count;
            }
            cv_.notify_all();
        }
        
        void build_free_list()
        {
            free_ = threads_.get();
            for (std::size_t i = 0; i + 1 < free_count_; ++i)
            {
                threads_[i].next_ = &threads_[i + 1];
            }
            threads_[free_count_ - 1].next_ = nullptr;
        }
    private:
        std::unique_ptr<Node[]> threads_;
        Node* free_ = nullptr;
        std::size_t free_count_ = 0;

        std::mutex mtx_;
        std::condition_variable cv_;
    };

    thread_impl* do_new_thread(const char* name, c_tfunc tfunc = nullptr, void* tdata = nullptr, int ms = 0);

    inline thread_impl* do_get_main()
    {
        return static_cast<thread_impl*>(athd_getmain());
    }

    // ---------------------------- 模块数据 ----------------------------------
    class mdata
    {
    public:
        mdata()
        {
            // 在构造中设置默认容量
            nodes_.init(default_job_capecity);
            jobs_.init(default_job_capecity);
        }

        ~mdata()
        {
            athd::waitstops();
        }

    public:
        list<node> nodes_;
        list<athd::pvt::job> jobs_;

        std::recursive_mutex mtx_;

        std::vector<thread_impl*> pthreads_;
        std::vector<std::unique_ptr<thread_impl>> threads_;
        std::vector<std::unique_ptr<pool_impl>> pools_;
        std::unordered_map<std::uint64_t, thread_impl*> thread_map_;
        std::vector<thread_impl*> lua_threads_;
    };

    mdata* get_mdata();

    #ifdef _WIN64
    void setmainready();
    #endif
}