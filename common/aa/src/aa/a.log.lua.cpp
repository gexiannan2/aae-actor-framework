#include "ahcpp.h"

#define LUA_LIB

#include <lua.hpp>
#include <cstring>
#include "alua.h"
#include "alog.h"

namespace
{
    static auto lua_log_file = alog::addfile("lua");

    void lua_print(lua_State* L, int level)
    {
        constexpr std::size_t kMaxLog = 4096;            // 可调
        static thread_local char g_buff[kMaxLog];

        char* p   = g_buff;
        char* end = g_buff + kMaxLog - 16;           // 预留 "<truncated>" 空间
        int top   = lua_gettop(L);

        for (int i = 1; i <= top; ++i)
        {
            if (i > 1 && p < end)
            {
                *p++ = ' ';        // 空格分隔
            }

            switch (lua_type(L, i))
            {
            case LUA_TNUMBER:
                p += std::snprintf(p, end - p, "%.14g", lua_tonumber(L, i));
                break;
            case LUA_TSTRING:
            {
                std::size_t len;
                const char* s = lua_tolstring(L, i, &len);
                std::size_t cp = std::min(len, std::size_t(end - p));
                std::memcpy(p, s, cp);
                p += cp;
                break;
            }
            case LUA_TBOOLEAN:
                if (lua_toboolean(L, i))
                {
                    if (p + 4 < end)
                    {
                        std::memcpy(p, "true", 4);
                        p += 4;
                    }
                }
                else
                {
                    if (p + 5 < end)
                    {
                        std::memcpy(p, "false", 5);
                        p += 5;
                    }
                }                
                break;
            case LUA_TNIL:
                if (p + 3 <= end) { memcpy(p, "nil", 3); p += 3; }
                break;
            default:
                p += std::snprintf(p, end - p, "%s:%p",
                                lua_typename(L, lua_type(L, i)),
                                lua_topointer(L, i));
                break;
            }

            if (p >= end)
            {
                break;   // 提前截断
            }
        }

        if (p >= end)
        {
            static constexpr char kTrunc[] = " <truncated>";
            std::memcpy(p, kTrunc, sizeof(kTrunc) - 1);
            p += sizeof(kTrunc) - 1;
        }

        std::size_t final_len = p - g_buff;
        alog_print(lua_log_file, level, g_buff, final_len);
    }

    static int lua_api_info(lua_State* L)
    {
        lua_print(L, alog::LogLevel::LEVEL_INFO);
        return 0;
    }

    static int lua_api_debug(lua_State* L)
    {
        lua_print(L, alog::LogLevel::LEVEL_DEBUG);
        return 0;
    }

    static int lua_api_warning(lua_State* L)
    {
        lua_print(L, alog::LogLevel::LEVEL_WARNING);
        return 0;
    }

    static int lua_api_error(lua_State* L)
    {
        lua_print(L, alog::LogLevel::LEVEL_ERROR);
        return 0;
    }

    static auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            static const luaL_Reg alog_funcs[] = {
                {"info", lua_api_info},
                {"debug", lua_api_debug},
                {"warning", lua_api_warning},
                {"error", lua_api_error},
                {"setroot", alua::tocfunc<alog_setroot>()},
                {"setoutdebug", alua::tocfunc<alog_setoutdebug>()},
                {"print_screen", alua::tocfunc<alog_print_screen>()},
                {"print_syslog", alua::tocfunc<alog_set_print_sys_log_to_screen>()},
                {"print_stack", alua::tocfunc<alog_print_stack>()},
                {"addfile", alua::tocfunc<alog_addfile>()},
                {NULL, NULL}
            };

            alua::regmod(L, "alog", alog_funcs);
        }
    );

}

AA_API void* alua_getlogfile()
{
    return lua_log_file;
}
