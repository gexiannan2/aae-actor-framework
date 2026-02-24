/**********************************************************************************
 * 
 * AA分布式引擎（C++）
 * 
 * 自动化 Lua ⟷ C API功能模块
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 **********************************************************************************/

 /**********************************************************************************
 *  1、Lua->C: 自动推导和注册C-Func参数和返回值给Lua调用
 *      函数 alua::reg_func<c-func>(L, mod_name, func_name)
 * 
 *  2、C->Lua: 自动推导C-Call语句的参数和返回值并调用Lua函数
 *
 *      无返回值调用（常用）
 *          alua::call(L, mod_name, func_name, lua-args...);
 * 
 *      显式返回类型调用（常用）
 * 
 *          int ret = alua::call(L, mod_name, func_name, lua-args...);
 * 
 *          std::tuple<int, bool, string> ret = alua::call(L, mod_name, func_name, lua-args...);
 * 
 *          std::map<string, int> ret = alua::call(L, mod_name, func_name, lua-args...);
 * 
 *      隐式返回类型调用（需声明与LUA函数一致的c-func声明）
 *          auto ret = alua::call<c-func>(...);
 * 
 * 参数和返回值类型支持：
 *      1、基础类型：int/uint32/int64/uint64/string(const char*)/bool
 *      2、数组：std::vector<基础类型>
 *      3、元组：std::tuple<基础类型，基础类型，...>
 *      4、字典：std::map/std::unordered_map<基础类型, 基础类型>
 *
 * 
**********************************************************************************/

/******************************************
示例1：注册
 
void lua_boot(){ std::puts("boot"); }
alua::reg_func<lua_boot>(L, "sys", "boot");

// 例2：单值返回
int lua_add(int a, int b){ return a + b; }
alua::reg_func<lua_add>(L, "math", "add");

// 例3：tuple 返回
std::tuple<int, std::string> lua_pair(int x){
    return {x * 2, "ok"};
}
alua::reg_func<lua_pair>(L, "cfg", "pair");

// 例4：vector 返回
std::vector<std::string> lua_list(){
    return {"a","b","c"};
}
alua::reg_func<lua_list>(L, "db", "list");

// 例5：unordered_map 返回
std::unordered_map<std::string, int> lua_conf(){
    return {{"width",800},{"height",600}};
}
alua::reg_func<lua_conf>(L, "cfg", "conf");

******************************************
示例2：从 C++ 调用 Lua 函数（无函数指针方式）

// 例6：无参无返回
alua::call(L, "main");

// 例7：单值返回
int r = alua::call<int>(L, "math", "add", 10, 20);

// 例8：tuple 返回
auto [i, s] = alua::call<std::tuple<int, std::string>>(L, "cfg", "pair", 5);

// 例9：vector 返回
auto vec = alua::call<std::vector<std::string>>(L, "db", "list");

// 例10：map 返回
auto mp = alua::call<std::unordered_map<std::string, int>>(L, "cfg", "conf");

******************************************
示例3：从 C++ 调用 Lua 函数（无函数指针方式）
// 例6：无参无返回
alua::call(L, "main");

// 例7：单值返回
int r = alua::call<int>(L, "math", "add", 10, 20);

// 例8：tuple 返回
auto [i, s] = alua::call<std::tuple<int, std::string>>(L, "cfg", "pair", 5);

// 例9：vector 返回
auto vec = alua::call<std::vector<std::string>>(L, "db", "list");

// 例10：map 返回
auto mp = alua::call<std::unordered_map<std::string, int>>(L, "cfg", "conf");

*/

#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <source_location>
#include <iostream>

#include "aos.h"
#include "ahar.h"
#include "athd.h"
#include "alog.h"
#include "lua.hpp"

using lua_init_func = void(*)(lua_State*);

AA_API void alua_require(void* L, const char* mod_name);
AA_API void alua_inccallcount();
AA_API std::uint64_t alua_getcallcount();
AA_API void* alua_getcurrstate();
AA_API void* alua_newstate(bool setpaths = true);
AA_API void alua_closestate();
AA_API void alua_loadfile(const char* fname);
AA_API void alua_addpaths(const char* path, bool reset = false);
AA_API void alua_addcpaths(const char* path, bool reset = false);
AA_API void* alua_getlogfile();
AA_API bool alua_addinitfunc(void* fn);
AA_API void alua_resetpaths();

namespace alua
{
    inline std::string currtraceback()
    {
        lua_State* L = static_cast<lua_State*>(alua_getcurrstate());

        lua_getglobal(L, "debug");
        lua_getfield(L, -1, "traceback");
        lua_pushnil(L);
        lua_pushinteger(L, 2);

        lua_call(L, 2, 1);
        std::string tb = lua_tostring(L, -1) ? : "";
        lua_pop(L, 2);
        return tb;
    }

    template<typename... Args>
    inline void info(const char* fmt, Args... args)
    {
        static auto lf = alua_getlogfile();
        alog::pvt::print(lf, alog::LEVEL_INFO, fmt, args...);
    }

    template<typename... Args>
    inline void debug(const char* fmt, Args... args)
    {
        if (!alog_is_print_debug())
        {
            return;
        }
        static auto lf = alua_getlogfile();
        alog::pvt::print(lf, alog::LEVEL_DEBUG, fmt, args...);
    }

    template<typename... Args>
    inline void warning(const char* fmt, Args... args)
    {
        static auto lf = alua_getlogfile();
        alog::pvt::print(lf, alog::LEVEL_WARNING, fmt, args...);
    }

    template<typename... Args>
    inline void error(const char* fmt, Args... args)
    {
        static auto lf = alua_getlogfile();
        auto s = std::string(fmt) + "\r\n" + currtraceback();
        alog::pvt::print(lf, alog::LEVEL_ERROR, s.data(), args...);
    }

    inline void require(const char* mod_name)
    {
        alua_require(alua_getcurrstate(), mod_name);
    }

    inline std::uint64_t getcallcount()
    {
        return alua_getcallcount();
    }    

    inline lua_State* newstate(bool setpaths = true)
    {
        return static_cast<lua_State*>(alua_newstate(setpaths));
    }

    inline lua_State* getcurrstate()
    {
        return static_cast<lua_State*>(alua_getcurrstate());
    }

    inline void closestate()
    {
        alua_closestate();
    }

    inline void loadfile(const char* main)
    {
        alua_loadfile(main);
    }

    inline alog::file* getlogfile()
    {
        return static_cast<alog::file*>(alua_getlogfile());
    }

    inline bool addinitfunc(lua_init_func fn)
    {
        return alua_addinitfunc(reinterpret_cast<void*>(fn));
    }
    
    // 工具：把 __PRETTY_FUNCTION__ 里的裸函数名取出来
    namespace pvt
    {
        // 判断是否是 tuple
        template<typename T> inline constexpr bool is_tuple_v = false;
        template<typename... Ts> inline constexpr bool is_tuple_v<std::tuple<Ts...>> = true;

        // 把“函数类型”→“函数指针”，“指针/值”保持原样
        template<auto> struct func_ptr { static constexpr auto value = func_ptr{}; };
        template<typename R, typename... A, R (*f)(A...)>
        struct func_ptr<f> { static constexpr auto value = f; };

        // 计算期望返回数量
        template<typename R>
        constexpr int expected_returns() noexcept
        {
            if constexpr (std::is_same_v<R, void>) return 0;
            else if constexpr (is_tuple_v<R>) return std::tuple_size_v<R>;
            else return 1;
        }

        constexpr std::string_view raw_name(std::string_view pretty) noexcept
        {
            std::size_t tail = pretty.rfind('(');
            if (tail == std::string_view::npos) { return pretty; }
            std::size_t head = pretty.rfind(' ', tail);
            return (head == std::string_view::npos)
                   ? pretty.substr(0, tail)
                   : pretty.substr(head + 1, tail - head - 1);
        }

        // 单值读取 / 压栈
        template<typename T> struct popper;
        template<typename T> struct pusher;

        // 数值
        template<> struct popper<int>           { static int           get(lua_State* l,int i){return static_cast<int>(luaL_checkinteger(l,i));}};
        template<> struct popper<float>         { static float         get(lua_State* l,int i){return static_cast<float>(luaL_checknumber(l,i));}};
        template<> struct popper<double>        { static double        get(lua_State* l,int i){return luaL_checknumber(l,i);}};
        template<> struct popper<bool>          { static bool          get(lua_State* l,int i){return lua_toboolean(l,i)!=0;}};
        template<> struct popper<std::uint32_t> { static std::uint32_t get(lua_State* l,int i){return static_cast<std::uint32_t>(luaL_checkinteger(l,i));}};
        template<> struct popper<std::uint64_t> { static std::uint64_t get(lua_State* l,int i){return static_cast<std::uint64_t>(luaL_checkinteger(l,i));}};
        template<> struct popper<std::int64_t>  { static std::int64_t  get(lua_State* l,int i){return static_cast<std::int64_t>(luaL_checkinteger(l,i));}};

        // 字符串
        template<> struct popper<std::string>
        {
            static std::string get(lua_State* l,int i)
            {
                std::size_t len = 0;
                const char* s = luaL_checklstring(l,i,&len);
                return std::string{s,len};
            }
        };
        template<> struct popper<std::string*>
        {
            static std::string* get(lua_State* l,int i)
            {
                std::size_t len;
                auto s = luaL_checklstring(l, i, &len);
                return new std::string{s, len};
            }
        };
        template<> struct popper<const char*> { static const char* get(lua_State* l,int i){return luaL_checkstring(l,i);}};

        // 轻量 userdata
        template<> struct popper<void*> { static void* get(lua_State* l,int i){return lua_touserdata(l,i);}};

        template<> struct popper<std::string_view> 
        {
            static std::string_view get(lua_State* l,int i)
            {
                size_t size;
                auto data = luaL_checklstring(l,i,&size);
                return std::string_view{data, size};
            }
        };

        template<>
        struct popper<const std::string_view&> {
            static std::string_view get(lua_State* L, int idx) {
                std::size_t len;
                const char* s = luaL_checklstring(L, idx, &len);
                return std::string_view{s, len};   // 按值返回即可
            }
        };

        template<> struct popper<athd::thread*> { static athd::thread* get(lua_State* l,int i){return static_cast<athd::thread*>(lua_touserdata(l,i));}};

        template<> struct popper<ref_lobj> 
        {
            static ref_lobj get(lua_State* l,int i)
            {
                ref_lobj res;
                res.obj_ = (void*)lua_topointer(l, i);
                lua_pushvalue(l, i);                
                res.idx_ = luaL_ref(alua::getcurrstate(), LUA_REGISTRYINDEX);
                return res;
            }
        };

        // 压栈
        template<> struct pusher<int>           { static void put(lua_State* l,int v){lua_pushinteger(l,v);}};
        template<> struct pusher<float>         { static void put(lua_State* l,float v){lua_pushnumber(l,v);}};
        template<> struct pusher<double>        { static void put(lua_State* l,double v){lua_pushnumber(l,v);}};
        template<> struct pusher<bool>          { static void put(lua_State* l,bool v){lua_pushboolean(l,v);}};
        template<> struct pusher<std::uint32_t> { static void put(lua_State* l,std::uint32_t v){lua_pushnumber(l,v);}};
        template<> struct pusher<std::uint64_t> { static void put(lua_State* l,std::uint64_t v){lua_pushnumber(l,v);}};
        template<> struct pusher<std::int64_t>  { static void put(lua_State* l,std::int64_t v){lua_pushnumber(l,v);}};
        template<> struct pusher<std::string>   { static void put(lua_State* l,const std::string& v){lua_pushlstring(l,v.data(),v.size());}};
        template<> struct pusher<const char*>   { static void put(lua_State* l,const char* v){lua_pushstring(l,v);}};
        template<> struct pusher<void*>         { static void put(lua_State* l,void* v){lua_pushlightuserdata(l,v);}};
        template<> struct pusher<std::string*>   { static void put(lua_State* l,std::string*v){lua_pushlstring(l,v->data(),v->size());}};
        template<> struct pusher<std::string_view>   { static void put(lua_State* l,const std::string_view& v){lua_pushlstring(l,v.data(),v.size());}};
        template<> struct pusher<athd::thread*>   { static void put(lua_State* l,athd::thread* v){lua_pushlightuserdata(l,v);}};

        // vector
        template<typename T> struct popper<std::vector<T>>
        {
            static std::vector<T> get(lua_State* l,int idx)
            {
                luaL_checktype(l,idx,LUA_TTABLE);
                std::vector<T> v;
                lua_pushnil(l);
                while(lua_next(l,idx)!=0)
                {
                    v.emplace_back(popper<T>::get(l,-1));
                    lua_pop(l,1);
                }
                return v;
            }
        };
        template<typename T> struct pusher<std::vector<T>>
        {
            static void put(lua_State* l,const std::vector<T>& v)
            {
                lua_createtable(l,static_cast<int>(v.size()),0);
                for(std::size_t i=0;i<v.size();++i)
                {
                    pusher<T>::put(l,v[i]);
                    lua_rawseti(l,-2,static_cast<int>(i+1));
                }
            }
        };

        // unordered_map
        template<typename K,typename V> struct popper<std::unordered_map<K,V>>
        {
            static std::unordered_map<K,V> get(lua_State* l,int idx)
            {
                luaL_checktype(l,idx,LUA_TTABLE);
                std::unordered_map<K,V> m;
                lua_pushnil(l);
                while(lua_next(l,idx)!=0)
                {
                    K k=popper<K>::get(l,-2);
                    V v=popper<V>::get(l,-1);
                    m.emplace(std::move(k),std::move(v));
                    lua_pop(l,1);
                }
                return m;
            }
        };
        template<typename K,typename V> struct pusher<std::unordered_map<K,V>>
        {
            static void put(lua_State* l,const std::unordered_map<K,V>& m)
            {
                lua_createtable(l,0,static_cast<int>(m.size()));
                for(const auto& [k,v]:m)
                {
                    pusher<K>::put(l,k);
                    pusher<V>::put(l,v);
                    lua_settable(l,-3);
                }
            }
        };

        // tuple
        template<typename...> struct tuple_popper;
        template<> struct tuple_popper<> { static std::tuple<> get(lua_State*,int){return {};}; };
        template<typename H,typename... T> struct tuple_popper<H,T...>
        {
            static auto get(lua_State* l,int idx)
            {
                return std::tuple_cat(std::make_tuple(popper<H>::get(l,idx)),
                                      tuple_popper<T...>::get(l,idx+1));
            }
        };

        // 返回值包装
        template<typename R> struct ret_helper
        {
            static int push(lua_State* l,const R& v){pusher<R>::put(l,v);return 1;}
        };
        template<> struct ret_helper<void>{ static int push(lua_State*,...){return 0;} };
        template<typename... Ts> struct ret_helper<std::tuple<Ts...>>
        {
            static int push(lua_State* l,const std::tuple<Ts...>& tup)
            {
                std::apply([&l](const Ts&... vs){(pusher<Ts>::put(l,vs),...);},tup);
                return static_cast<int>(sizeof...(Ts));
            }
        };

        // C 函数 → Lua 包装
        template<auto f> struct lua_to_c_wrapper;
        template<typename R,typename... A,R(*f)(A...)> struct lua_to_c_wrapper<f>
        {
            static int call(lua_State* l)
            {
                constexpr int k_expect=sizeof...(A);
                const int argc=lua_gettop(l);
                if(argc!=static_cast<int>(k_expect))
                {
                    luaL_error(l,"expect %d args, got %d",k_expect,argc);
                }
                try
                {
                    auto args=tuple_popper<A...>::get(l,1);
                    if constexpr(std::is_same_v<R,void>)
                    {
                        std::apply(f,args);
                        return 0;
                    }
                    else
                    {
                        R ret=std::apply(f,args);
                        return ret_helper<R>::push(l,ret);
                    }
                }
                catch(const std::exception& e)
                {
                    luaL_where(l,1);
                    lua_pushfstring(l,"C++ exception: %s",e.what());
                    lua_concat(l,2);
                    lua_error(l);
                }
                catch(...)
                {
                    luaL_where(l,1);
                    lua_pushstring(l,"unknown C++ exception");
                    lua_concat(l,2);
                    lua_error(l);
                }
                return 0;
            }
        };

        // 已知函数指针的调用实现
        template<auto f> struct lua_caller;
        template<typename R,typename... A,R(*f)(A...)> struct lua_caller<f>
        {
            static R call(lua_State* L,const char* mod,const char* fn,const A&... args)
            {
                constexpr int k_argc=sizeof...(A);
                constexpr int k_ret = expected_returns<R>();
                constexpr bool k_void=std::is_same_v<R,void>;
                const int base=lua_gettop(L);

                lua_getglobal(L,"debug");
                lua_getfield(L,-1,"traceback");

                // 找函数
                if(mod && *mod)
                {
                    lua_getglobal(L,mod);
                    if(lua_isnil(L, -1))
                    {
                        luaL_error(L,"module '%s' not found",mod);
                    }
                    if(!lua_istable(L,-1))
                    {
                        luaL_error(L,"global '%s' is not table",mod);
                    }
                    lua_getfield(L,-1,fn);
                    lua_remove(L,-2);
                }
                else
                {
                    lua_getglobal(L,fn);
                }
                if(!lua_isfunction(L,-1))
                {
                    auto full=mod?(std::string(mod)+'.'+fn):std::string(fn);
                    alog_print(getlogfile(), alog::LogLevel::LEVEL_ERROR, full.c_str(), full.size());
                }

                (pusher<std::decay_t<A>>::put(L,args),...);

                alua_inccallcount();
                if(lua_pcall(L,k_argc,k_ret,base+2)!=LUA_OK)
                {
                    size_t sz;
                    auto str=lua_tolstring(L,-1,&sz);
                    std::string s;
                    if (mod)
                    {
                        s = "调用函数：" + s + "." + std::string(fn);
                    }
                    else
                    {
                        s = "调用函数：" + std::string(fn);
                    }
                    s = s + "发生异常：" + std::string(str, sz);
                    alog_print(getlogfile(), alog::LogLevel::LEVEL_ERROR, s.data(), s.size());
                }

                if constexpr(std::is_same_v<R,void>)
                {
                    lua_settop(L,base);
                    return;
                }
                else if constexpr(is_tuple_v<R>)
                {
                    // 读取 tuple 返回值
                    R ret = [&L,k_ret] {
                        return popper<R>::get(L,-k_ret);
                    }();
                    lua_settop(L,base);
                    return ret;
                }
                else
                {
                    R ret=popper<R>::get(L,-1);
                    lua_settop(L,base);
                    return ret;
                }
            }
        };

        // 无需函数指针的调用实现
        template<typename R>
        struct free_caller
        {
            template<typename... Args>
            static R call(lua_State* L,const char* mod,const char* fn,Args&&... args)
            {
                constexpr int k_ret = expected_returns<R>();
                constexpr bool k_void=std::is_same_v<R,void>;
                const int base=lua_gettop(L);

                lua_getglobal(L,"debug");
                lua_getfield(L,-1,"traceback");

                // 找函数
                if(mod && *mod)
                {
                    lua_getglobal(L,mod);
                    if(lua_isnil(L, -1))
                    {
                        luaL_error(L,"module '%s' not found",mod);
                    }
                    if(!lua_istable(L,-1))
                    {
                        luaL_error(L,"global '%s' is not table",mod);
                    }
                    lua_getfield(L,-1,fn);
                    lua_remove(L,-2);
                }
                else
                {
                    lua_getglobal(L,fn);
                }
                if(!lua_isfunction(L,-1))
                {
                    auto full=mod?(std::string(mod)+'.'+fn):std::string(fn);
                    alog_print(getlogfile(), alog::LogLevel::LEVEL_ERROR,full.c_str(),full.size());
                }

                (pusher<std::decay_t<Args>>::put(L,std::forward<Args>(args)),...);

                alua_inccallcount();
                if(lua_pcall(L,sizeof...(Args),k_ret,base+2)!=LUA_OK)
                {
                    size_t sz;
                    auto str=lua_tolstring(L,-1,&sz);
                    std::string s;
                    if (mod)
                    {
                        s = "调用函数：" + s + "." + std::string(fn);
                    }
                    else
                    {
                        s = "调用函数：" + std::string(fn);
                    }
                    s = s + "发生异常：" + std::string(str, sz);
                    alog_print(getlogfile(), alog::LogLevel::LEVEL_ERROR, s.data(), s.size());
                }

                if constexpr(std::is_same_v<R,void>)
                {
                    lua_settop(L,base);
                    return;
                }
                else if constexpr(is_tuple_v<R>)
                {
                    R ret = popper<R>::get(L,-k_ret);
                    lua_settop(L,base);
                    return ret;
                }
                else
                {
                    R ret=popper<R>::get(L,-1);
                    lua_settop(L,base);
                    return ret;
                }
            }
        };

        // tuple 读取器
        template<typename... Ts>
        struct popper<std::tuple<Ts...>>
        {
            static std::tuple<Ts...> get(lua_State* l,int idx)
            {
                constexpr std::size_t N=sizeof...(Ts);
                if(lua_gettop(l)-idx+1<static_cast<int>(N))
                    luaL_error(l,"not enough return values for tuple");

                return [&l,idx]<std::size_t... I>(std::index_sequence<I...>)
                {
                    return std::make_tuple(popper<Ts>::get(l,idx+I)...);
                }(std::make_index_sequence<N>{});
            }
        };
    } // namespace

    // 已知函数原型，两参
    template<auto f,typename... Args>
    auto call(const char* fn,Args&&... args)
    {
        return pvt::lua_caller<f>::call(getcurrstate(),nullptr,fn,std::forward<Args>(args)...);
    }
    // 已知函数原型，三参
    template<auto f,typename... Args>
    auto call(const char* mod,const char* fn,Args&&... args)
    {
        return pvt::lua_caller<f>::call(getcurrstate(),mod,fn,std::forward<Args>(args)...);
    }
    // 仅指定返回值，两参（默认 void）
    template<typename R=void,typename... Args>
    auto call(const char* fn,Args&&... args)
    {
        return pvt::free_caller<R>::call(getcurrstate(),nullptr,fn,std::forward<Args>(args)...);
    }
    // 仅指定返回值，三参
    template<typename R=void,typename... Args>
    auto call(const char* mod,const char* fn,Args&&... args)
    {
        return pvt::free_caller<R>::call(getcurrstate(),mod,fn,std::forward<Args>(args)...);
    }

    // 注册函数
    template<auto f>
    inline void reg_func(const char* mod_name,const char* lua_name=nullptr)
    {
        auto L = getcurrstate();
        lua_getglobal(L,mod_name);
        if(!lua_istable(L,-1))
        {
            lua_pop(L,1);
            luaL_error(L,"module '%s' not found",mod_name);
            return;
        }
        const char* real_name=lua_name?lua_name:pvt::raw_name(std::source_location::current().function_name()).data();
        lua_pushcfunction(L, pvt::lua_to_c_wrapper<f>::call);
        lua_setfield(L,-2,real_name);
        lua_pop(L,1);
    }

    template<auto f>
    inline lua_CFunction tocfunc()
    {
        return pvt::lua_to_c_wrapper<f>::call;
    }
    
    inline bool regmod(lua_State* L, const char* mod_name, const luaL_Reg regs[])
    {
        lua_getglobal(L, mod_name);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, mod_name);
        }
        
        // 修改循环变量（添加 const）
        for (const luaL_Reg* reg = regs; reg->name != NULL; ++reg) {
            lua_pushcfunction(L, reg->func);
            lua_setfield(L, -2, reg->name);
        }
        
        lua_pop(L, 1);
        return true;
    }
    
}