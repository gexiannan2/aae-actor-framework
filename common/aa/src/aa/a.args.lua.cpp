#include "ahcpp.h"

#include "alua.h"
#include "a.args.h"


namespace
{
    void lua_arger_to_lua(lua_State* L, int tabidx, aargs::arger_impl* arger)
    {       
        if (!arger)
        {
            alua::error("args.reload: arger指针为空");
            return;
        }
        lua_pushvalue(L, tabidx);

        auto set_field = [&](const char* key, auto&& val)
        {
            lua_pushstring(L, key);
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::int64_t>)
            {
                lua_pushinteger(L, static_cast<lua_Integer>(val));
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                lua_pushboolean(L, val);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                lua_pushstring(L, val.c_str());
            }
            else if constexpr (std::is_same_v<T, std::vector<std::int64_t>> ||
                            std::is_same_v<T, std::vector<bool>> ||
                            std::is_same_v<T, std::vector<std::string>>)
            {
                lua_newtable(L);
                for (size_t i = 0; i < val.size(); ++i)
                {
                    lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
                    if constexpr (std::is_same_v<T, std::vector<std::int64_t>>)
                    {
                        lua_pushinteger(L, static_cast<lua_Integer>(val[i]));
                    }
                    else if constexpr (std::is_same_v<T, std::vector<bool>>)
                    {
                        lua_pushboolean(L, val[i]);
                    }
                    else if constexpr (std::is_same_v<T, std::vector<std::string>>)
                    {
                        lua_pushstring(L, val[i].c_str());
                    }
                    lua_settable(L, -3);
                }
            }
            lua_settable(L, -3);
        };

        for (const auto& [k, v] : arger->i64_)
        {
            set_field(k.c_str(), v);
        }
        for (const auto& [k, v] : arger->b_)
        {
            set_field(k.c_str(), v);
        }
        for (const auto& [k, v] : arger->s_)
        {
            set_field(k.c_str(), v);
        }
        for (const auto& [k, v] : arger->vi64_)
        {
            set_field(k.c_str(), v);
        }
        for (const auto& [k, v] : arger->vb_)
        {
            set_field(k.c_str(), v);
        }
        for (const auto& [k, v] : arger->vs_)
        {
            set_field(k.c_str(), v);
        }
    }

    static int lua_newarger(lua_State* L)
    {
        auto pc = lua_tostring(L, 1);
        if (!pc)
        {
            alua::error("创建arger未指定文件名");
            return 0;
        }
        auto fname = std::string(pc);
        if (fname.find("runargs.txt") != std::string::npos)
        {
            lua_pushlightuserdata(L, aargs::getrunarger());
        }
        else
        {
            lua_pushlightuserdata(L, aargs::newarger());
        }
        return 1;
    }

    static int lua_load(lua_State* L)
    {
        auto arger = static_cast<aargs::arger_impl*>(lua_touserdata(L, 2));
        auto fname = lua_tostring(L, 3);
        
        aargs_load(arger, fname);
        lua_arger_to_lua(L, 1, arger);

        return 0;
    }

    static int lua_reload(lua_State* L)
    {
        auto arger = static_cast<aargs::arger_impl*>(lua_touserdata(L, 2));        
        aargs_reload(arger);
        lua_arger_to_lua(L, 1, arger);
        return 0;
    }

    static int lua_set(lua_State* L)
    {
        auto arger = static_cast<aargs::arger*>(lua_touserdata(L, 1)); 
        if (!arger)
        {
            alua::error("args.set: arger指针为空");
            return 0;
        }
        
        auto key = lua_tostring(L, 2);
        auto vt = lua_type(L, 3);
        switch (vt)
        {
            case LUA_TNUMBER:
                arger->set(key, (std::int64_t)lua_tointeger(L, 3));
                break;
            case LUA_TSTRING:
                arger->set(key, lua_tostring(L, 3));
                break;
            case LUA_TBOOLEAN:
                arger->set(key, (bool)lua_toboolean(L, 3));
                break;
            default:
                alua::error("Args: 不支持的Value类型：{}-{}", key, lua_tostring(L, -3));
        }
        return 0;
    }

    static auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            static const luaL_Reg alog_funcs[] = {
                {"newarger", lua_newarger},
                {"load", lua_load},
                {"reload", lua_reload},
                {"set", lua_set},
                {NULL, NULL}
            };

            alua::regmod(L, "aargs", alog_funcs);
        }
    );
}
