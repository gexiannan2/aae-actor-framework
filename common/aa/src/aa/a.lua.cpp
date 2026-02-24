#include "ahcpp.h"

#include <mutex>

#include "afile.h"
#include "aargs.h"
#include "astr.h"

#include "a.lua.h"

namespace alua
{
    using lua_init_funcs = std::vector<lua_init_func>;

    class mdata
    {
    public:
        std::mutex mtx_;
        std::atomic_uint64_t call_count_ = 0;
        lua_init_funcs init_funcs_;
    };
    
    mdata* get_mdata()
    {
        static mdata md_;   // 首次调用时构造，顺序可预测
        return &md_;
    }

    auto _ = get_mdata();

    static thread_local lua_State* lua_curr_state_ = nullptr;
    static std::atomic_uint64_t lua_call_count_ = 0;
}

AA_API bool alua_addinitfunc(void* fn)
{
    auto md = alua::get_mdata();
    std::lock_guard<std::mutex> lock(md->mtx_);
    md->init_funcs_.push_back(reinterpret_cast<lua_init_func>(fn));
    return true;
}

AA_API void alua_inccallcount()
{
    alua::lua_call_count_++;
}

AA_API std::uint64_t alua_getcallcount()
{
    return alua::lua_call_count_.load();
}

AA_API void* alua_getcurrstate()
{
    return alua::lua_curr_state_;
}

void alua_resetpaths_by(lua_State* L)
{
    auto maindir = aargs::getrunarger()->get("maindir", "");
    if (maindir.empty())
    {
        alua::error("main.lua或对象runarger未设置主路径maindir");
        return;
    }

    auto arger = aargs::getrunarger();
    auto pathroots =  arger->get("pathroots", "");
    auto cpathroots =  arger->get("cpathroots", "");

    if (pathroots.empty() || cpathroots.empty())
    {
        //alua::error("runarger.txt未设置lua根目录pathroots/cpathroots");
        return;
    }

    auto build_path = [=](const std::string& root, const std::string& ext, const std::string exts) -> std::string
        {
            std::string ret;
            std::vector<std::string> dirs;
            auto vec = astr::split(root, ';');
            for (auto root : vec)
            {
                afile::listsubdirs(maindir + root, dirs, exts);
            }
            for (const auto& d : dirs)
            {
                if (!ret.empty() && ret[ret.size()-1] != ';')
                {
                    ret += ';';
                }
                ret += afile::normalizepath(d + "/?." + ext + ";");
            }
            return ret;
        };

    if(!pathroots.empty())
    {
        auto paths = maindir + "?.lua;" + build_path(pathroots, "lua", "lua");
        alua_addpaths(paths.c_str(), true);
    }
    if(!pathroots.empty())
    {
        #ifdef _WIN32
            alua_addcpaths(build_path(cpathroots, "dll", "dll").c_str(), true);
        #else
            alua_addcpaths(build_path(cpathroots, "so", "so").c_str(), true);
        #endif
    }
}

AA_API void alua_resetpaths()
{
    alua_resetpaths_by(static_cast<lua_State*>(alua_getcurrstate()));
}

AA_API void* alua_newstate(bool setpaths)
{
    lua_State* L = luaL_newstate();   // 创建新的 Lua 虚拟机
    if (!L)
    {
        alua::error("创建虚拟机失败");
        return nullptr;
    }

    alua::lua_curr_state_ = L;
    luaL_openlibs(L);

    if (setpaths)
    {
        alua_resetpaths_by(L);
    }
    
    #ifdef _WIN32
    lua_pushboolean(L, true);
    #else
    lua_pushboolean(L, false);
    #endif
    lua_setglobal(L, "iswin");

    return L;
}

AA_API void alua_require(void* l, const char* mod_name)
{
    auto L = static_cast<lua_State*>(l);
    int save = lua_gettop(L);

    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");

    lua_getglobal(L, "require");
    lua_pushstring(L, mod_name);

    alua_inccallcount();
    if (lua_pcall(L, 1, 1, save + 2) != LUA_OK)
    {
        size_t size;
        auto str = lua_tolstring(L, -1, &size);
        alog_print(alua_getlogfile(), alog::LogLevel::LEVEL_ERROR, str, size);
    }

    lua_settop(L, save); 
}

AA_API void alua_closestate()
{
    auto L = alua::lua_curr_state_;
    alua::lua_curr_state_ = nullptr;
    if (L)
    {
        lua_close(L);
    }
}

AA_API void alua_loadfile(const char* fname)
{
    auto L = alua::getcurrstate();
    lua_getglobal(L,"debug");
    lua_getfield(L,-1,"traceback");
    if (luaL_loadfile(L, fname))
    {
        alua::error("文件加载失败: {} \r\n{}", fname, lua_tostring(L, -1));
        return;
    }
    int err = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (err != LUA_OK)
    {
        auto err = lua_tostring(L, -1);
        alua::error("文件执行失败: {} \r\n{}", fname, err);
        std::cout << "文件执行失败: " << fname << "\r\n" << err << std::endl;
        return;
    }
}

void alua_do_addpaths(const char* name, const char* path, bool reset)
{
    lua_State* L = alua::lua_curr_state_;
    
    auto base = lua_gettop(L);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, name);

    std::string cur_path;
    if (!reset)
    {
        cur_path = lua_tostring(L, -1);
    }

    if (!cur_path.empty() && cur_path[cur_path.size()-1] != ';')
    {
        cur_path += ';';
    }
    cur_path += path;
    
    lua_pushstring(L, cur_path.c_str());
    lua_setfield(L, -3, name);

    lua_settop(L, base);
}

AA_API void alua_addpaths(const char* path, bool reset)
{
    alua_do_addpaths("path", path, reset);
}

AA_API void alua_addcpaths(const char* path, bool reset)
{
    alua_do_addpaths("cpath", path, reset);
}

static int l_add(lua_State *L)
{
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a + b);
    return 1;
}

/* 入口：模块被 require 时执行 */
AA_API int luaopen_libaa(lua_State *L)
{
    alua::lua_curr_state_ = L;

    luaL_checkversion(L);

    auto md = alua::get_mdata();
    for (auto fn : md->init_funcs_)
    {
        fn(L);
    }

    luaL_Reg libs[] = {
        { "add", l_add },
        { NULL, NULL }
    };
#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, libs);
#else
    luaL_register(L, "libaa", libs);
#endif
    return 1;
}


namespace
{
    auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            auto mod_name = "alua";
            
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, mod_name);

            alua::reg_func<alua_resetpaths>(mod_name, "resetpaths");
        }
    );
}