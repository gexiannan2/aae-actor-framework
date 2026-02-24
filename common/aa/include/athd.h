/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 异步任务调度模块
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

/*******************************************************
  athread.h

  依赖: athread.so

初始化函数：
  set_job_capecity
  new_conc_threads
  new_thread

注：
    *请在系统初始化阶段调用初始化函数
    *其他非初始化函数被调用以后将不能再调用初始化函数
    *即不支持运行时创建和销毁线程，请规划好线程结构

********************************************************/

#pragma once
#include <functional>
#include <cstdint>
#include <atomic>

#include "aos.h"

extern "C"
{

	using c_twork = void(*)(void* job_ptr);
	using c_tdone = c_twork;
	using c_tfunc = void(*)(void*, int flag);

	struct job_base
	{
		job_base* next_ = nullptr;
		uint64_t job_count_;
	};

}

AA_API void* athd_getresult(void);
AA_API void athd_setresult(void* v);
AA_API bool athd_jobend();
AA_API void* athd_allocjob(void);
AA_API void* athd_newthread(const char* name, c_tfunc tfunc, void* tdata, int ms);
AA_API void* athd_newpool(const char* name, std::size_t num, c_tfunc tfunc, void* tdata, int ms);
AA_API void* athd_getct(void);
AA_API std::uint64_t athd_getctid();
AA_API void* athd_getmain();
AA_API void  athd_pushtjob(void* t,
				const char* job_name,
				void* job_ptr,
				c_twork work_fn,
				c_tdone done_fn);
AA_API void  athd_pushpjob(void* pool,
				const char* job_name,
				std::uint64_t cid,
				void* job_ptr,
				c_twork work_fn,
				c_tdone done_fn);

// 直接对外的接口
AA_API void  athd_setjobcapecity(std::size_t v);	
AA_API void  athd_setjobtimeoutlimit(void* tp, std::size_t v);
AA_API void  athd_freejob(void* job);	
AA_API int   athd_getpendingcount(void* pool, std::uint64_t cid);
AA_API void  athd_waitstops(void);
AA_API const char* athd_gettname(std::uint64_t tid);
AA_API int athd_getpoolthreads(void* pool, void** threads, std::size_t* size);

// ---------------------------------------------------------------------------
//  C++ 封装
// ---------------------------------------------------------------------------
namespace athd
{
	namespace pvt
	{
		using tfunc = std::function<void()>;
		class tdata final
		{
		public:
			pvt::tfunc start_;
			pvt::tfunc stop_;
			std::atomic_uint32_t start_count_ = 0;
			std::atomic_uint32_t stop_count_ = 0;
		public:
			tdata(int num, pvt::tfunc start, pvt::tfunc stop)
			{
				start_ = start;
				stop_ = stop;
				start_count_ = num;
				stop_count_ = num;
			};
		};

		static inline void thread_func(void* p, int flag)
		{
			auto td = static_cast<tdata*>(p);
			if (flag == 1)
			{
				if (td->start_)
				{
					td->start_();
				}
				td->start_count_--;
				if (!td->start_count_)
				{
					td->start_ = nullptr;
				}
				return;
			}
			if (flag == 2)
			{
				if (td->start_)
				{
					td->stop_();
				}
				td->stop_count_--;
				if (!td->stop_count_)
				{
					td->stop_ = nullptr;
					delete td;
				}
			}
		}

		struct data
		{
		public:
			tfunc start_;
			tfunc stop_;
			data(tfunc start, tfunc stop)
			{
				start_ = start;
				stop_ = stop;
			};
		};

		struct job
		{			
			job() noexcept = default;
			job(job&&) noexcept = default;
			job& operator=(job&&) noexcept = default;
			job(const job&) = delete;
			job& operator=(const job&) = delete;
			
			job* next_ = nullptr;
			uint64_t job_count_;

			tfunc work_;
			tfunc done_;

			char null[16];
		};

		static inline job* alloc_job(const tfunc& w, const tfunc& d)
		{
			void* raw = athd_allocjob();
			job* j = new (raw) job{};

			j->work_ = std::move(w);
			j->done_ = std::move(d);

			return j;
		}

		// 归还任务对象到对象池
		static inline void free_job(job* j)
		{
			j->work_ = nullptr;
			j->done_ = nullptr;

			athd_freejob(j);
		}

		// 任务执行回调
		static void thread_work(void* job_ptr)
		{
			static_cast<job*>(job_ptr)->work_();
		}

		// 任务完成回调
		static void thread_done(void* job_ptr)
		{			
			auto j = static_cast<job*>(job_ptr);
			if (j->done_)
			{
				j->done_();
			}
			if (athd_jobend())
			{
				free_job(j);
			}
		}
	} // 匿名 namespace

	// -----------------------------------------------------------------------
	//  单线程封装
	// -----------------------------------------------------------------------
	class thread final
	{
	public:
		// 将任务压入当前线程
		inline void pushjob(const char* job_name, const pvt::tfunc& w, const pvt::tfunc& d = nullptr)
		{
			athd_pushtjob(this, job_name, pvt::alloc_job(w, d), pvt::thread_work, pvt::thread_done);
		}
	};

	// 创建线程
	//	   name： 线程名称
	//     ms: 执行任务超时告警阈值，单位：毫秒
	inline thread* newthread(const char* name, pvt::tfunc start = nullptr, pvt::tfunc stop = nullptr, int ms = 50)
	{
		return static_cast<thread*>(athd_newthread(name, pvt::thread_func, new pvt::tdata(1, start, stop), ms));
	}

	inline void* getresult(void)
	{
		return athd_getresult();
	}

	inline void setresult(void* v)
	{
		athd_setresult(v);
	}

	inline bool jobisok()
	{
		return athd_jobend();
	}

	//  获取当前线程
	inline thread* getct()
	{
		return static_cast<thread*>(athd_getct());
	}

	inline thread* getmain()
	{
		return static_cast<thread*>(athd_getmain());
	}

	inline std::uint64_t getctid()
	{
		return athd_getctid();
	}

	inline const char* getctname()
	{
		return athd_gettname(athd_getctid());
	}

	inline const char* gettname(std::uint64_t tid)
	{
		return athd_gettname(tid);
	}

	inline void setjobcapecity(std::size_t v)
	{
		setjobcapecity(v);
	}

	inline void  setjobtimeoutlimit(void* tp, std::size_t v)
	{
		setjobtimeoutlimit(tp, v);
	}

	// 把作业任务压到new_thread创建的所有线程中执行
	//    job_name：作业名称，用于日志
	//    w：作业工作函数
	//    d: 作业完成回调函数
	inline void pushjobtoall(const char* job_name, const pvt::tfunc& w, const pvt::tfunc& d = nullptr)
	{
		athd_pushtjob(nullptr, job_name, pvt::alloc_job(w, d), pvt::thread_work, pvt::thread_done);
	}

	// 把作业任务压到new_thread创建的所有线程中执行
	//    job_name：作业名称，用于日志
	//    w：作业工作函数
	//    d: 作业完成回调函数
	inline void pushjobtoalllua(const char* job_name, const pvt::tfunc& w, const pvt::tfunc& d = nullptr)
	{
		athd_pushtjob((void*)1, job_name, pvt::alloc_job(w, d), pvt::thread_work, pvt::thread_done);
	}

	// -----------------------------------------------------------------------
	//  并发线程池
	// -----------------------------------------------------------------------
	class pool final
	{
	public:
		// 向并发线程池推送任务
		// cid： 一致性ID（consistency-id），非0则相同的ID在同一个线程顺序执行，0为轮询选择线程执行
		inline void pushjob(const char* job_name,
		                     const pvt::tfunc& w,
							 const pvt::tfunc& d = nullptr,
		                     std::uint64_t cid = 0)
		{
			athd_pushpjob(this, job_name, cid, pvt::alloc_job(w, d), pvt::thread_work, pvt::thread_done);
		}

		// 查询指定一致性ID的未决任务数，0：为threads的所有未决作业任务总和
		inline int pending(std::uint64_t cid = 0)
		{
			return athd_getpendingcount(this, cid);
		}
	};

	// 创建并发线程池
	//	   name： 线程池名称
	//	   num: 线程数量
	//     ms: 执行任务超时告警阈值，单位：毫秒
	inline pool* newpool(const char* name, int num, pvt::tfunc start = nullptr, pvt::tfunc stop = nullptr, int ms = 50)
	{
		return static_cast<pool*>(athd_newpool(name, num, pvt::thread_func, new pvt::tdata(num, start, stop), ms));
	}

	inline void waitstops()
	{
		athd_waitstops();
	}

	inline std::vector<thread*> getpoolthreads(void* pool)
	{
		size_t size = 100;
        void* threads[size];
		std::vector<thread*> ret;
        athd_getpoolthreads(pool, threads, &size);
        for(auto i = 0; i < (int)size; i++)
        {
            ret.push_back(static_cast<athd::thread*>(threads[i]));
        }
		return ret;
	}

}
