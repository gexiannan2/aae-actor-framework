#include "ahcpp.h"

#include <lua.hpp>
#include <cstring>

#include "alua.h"
#include "aapp.h"
#include "alog.h"
#include "athd.h"

namespace
{
    auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            static const luaL_Reg alog_funcs[] = {
                {"run", alua::tocfunc<aapp_run>()},
                {NULL, NULL}
            };

            alua::regmod(L, "aapp", alog_funcs);
        }
    );

}
