#include "ahcpp.h"

#include <lua.hpp>
#include <cstring>

#include "alua.h"
#include "aid.h"


namespace aid
{
    static int lua_crc32(lua_State* L)
    {
        size_t ln;
        auto s = luaL_checklstring(L, -1, &ln);
        if (!ln)
        {
            alua::error("请输入CRC32的字符串");
        }
        lua_pushinteger(L, aid_crc32(s, ln));
        return 1;
    }

    auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            static const luaL_Reg alog_funcs[] = {
                {"crc32", lua_crc32},
                {"gen", alua::tocfunc<aid_gen>()},                
                {NULL, NULL}
            };

            alua::regmod(L, "aid", alog_funcs);
        }
    );

}
