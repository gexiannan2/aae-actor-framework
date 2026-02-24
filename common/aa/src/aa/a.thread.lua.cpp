#include "ahcpp.h"

#include <lua.hpp>
#include <cstring>

#include "alua.h"
#include "athd.h"
#include "a.thread.h"

namespace
{
    void on_lua_tstart(void* t)
    {
        alua::newstate();
    }

    void on_lua_tstop(void* t)
    {
        alua::closestate();
    }

    void* new_thread(const char* thd_name, const char* entry_file, int ms)
    {
        auto ret = athd::newthread(thd_name,
                            [ef=std::string(entry_file)]()
                            {
                                alua::newstate();
                                alua::loadfile(ef.c_str());
                            },
                            []()
                            {
                                alua::closestate();
                            },
                            ms);
        auto md = athd::get_mdata();
        std::lock_guard<std::recursive_mutex> lk(md->mtx_);
        md->lua_threads_.push_back(static_cast<athd::thread_impl*>((void*)ret));
        return ret;
    }

    void* new_pool(const char* thd_name, const char* entry_file, int num, int ms)
    {
        auto ret = athd::newpool(thd_name,
                        num,
                        [ef=std::string(entry_file)]()
                        {
                            alua::newstate();
                            alua::loadfile(ef.c_str());
                        },
                        []()
                        {
                            alua::closestate();
                        },
                        ms
                    );
        auto md = athd::get_mdata();
        std::lock_guard<std::recursive_mutex> lk(md->mtx_);
        size_t size = 100;
        void* threads[size];
        athd_getpoolthreads(ret, threads, &size);
        for(auto i = 0; i < (int)size; i++)
        {
            md->lua_threads_.push_back(static_cast<athd::thread_impl*>(threads[i]));
        }
        
        return ret;
    }

    static int lua_pushjob(int ispool, std::uint64_t cid, int sidx, lua_State* L)
    {
        auto p = lua_topointer(L, sidx++);
        if (!p)
        {
            alua::error("没有给定线程或线程池");
            return 0;
        }
        auto job_id = (std::uint64_t)lua_tointeger(L, sidx++);
        auto job_name = lua_tostring(L, sidx++);
        if (!job_name)
        {
            alua::error("没有给定作业名称");
            return 0;
        }
        std::size_t ln;
        auto tfunc = lua_tolstring(L, sidx++, &ln);
        if (!tfunc)
        {
            alua::error("没有给定作业函数");
            return 0;
        }
        std::string stfunc(tfunc, ln);
        
        std::string sargs;
        auto args = lua_tolstring(L, sidx++, &ln);
        if(args)
        {
            sargs.assign(args, ln);
        }

        auto work_fn = [job_id, sargs, stfunc, sjob_name=std::string(job_name)]()
            {
                if (!job_id)
                {
                    alua::call("athd", "onwork", sjob_name, stfunc, sargs);
                    return;
                }
                auto ret = alua::call<std::string*>("athd", "onwork", sjob_name, stfunc, sargs);
                athd::setresult(ret);
            };

        athd::pvt::tfunc done_fn = [job_id]()
            {
                if (!job_id)
                {
                    alua::call("athd", "ondone", job_id);
                    return;
                }
                auto sres = static_cast<std::string*>(athd::getresult());
                alua::call("athd", "ondone", job_id, sres);
                delete sres;
            };
        if(!job_id)
        {
            done_fn = nullptr;
        }

        if (!ispool)
        {
            auto t = static_cast<athd::thread*>((void*)p);
            if (!t)
            {
                alua::error("无效线程对象");
                return 0;
            }
            t->pushjob(
                job_name,
                work_fn,
                done_fn
            );
            return 0;
        }

        auto pl = static_cast<athd::pool*>((void*)p);
        if (!pl)
        {
            alua::error("无效线程池对象");
            return 0;
        }
        pl->pushjob(
            job_name,
            work_fn,
            done_fn,
            cid
        );
        return 0;
    }

    static int lua_pushtjob(lua_State* L)
    {
        lua_pushjob(false, 0, 1, L);
        return 0;
    }

    static int lua_pushpjob(lua_State* L)
    {
        lua_pushjob(true, 0, 1, L);
        return 0;
    }

    static int lua_pushpjobby(lua_State* L)
    {
        lua_pushjob(true, (std::uint64_t)lua_tointeger(L, 1), 2, L);
        return 0;
    }

    auto _ = alua::addinitfunc(
        [](lua_State* L)
        {            
            static const luaL_Reg alog_funcs[] = {
                {"getctname", alua::tocfunc<athd::getctname>()},
                {"getmain", alua::tocfunc<athd_getmain>()},
                {"getct", alua::tocfunc<athd_getct>()},
                {"getctid", alua::tocfunc<athd_getctid>()},
                {"newthread", alua::tocfunc<new_thread>()},
                {"newpool", alua::tocfunc<new_pool>()},
                {"setjobtimeoutlimit", alua::tocfunc<athd_setjobtimeoutlimit>()},
                {"setjobcapecity", alua::tocfunc<athd_setjobcapecity>()},
                {"pushtjob", lua_pushtjob},
                {"pushpjob", lua_pushpjob},
                {"pushpjobby", lua_pushpjobby},
                {NULL, NULL}
            };

            alua::regmod(L, "athd", alog_funcs);
        }
    );

}
