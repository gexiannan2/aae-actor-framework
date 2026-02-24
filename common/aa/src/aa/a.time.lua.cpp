#include "ahcpp.h"

#include "alua.h"
#include "atime.h"

namespace atime
{
    auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            static const luaL_Reg alog_funcs[] = {
                {"nsec", alua::tocfunc<nsec>()},
                {"usec", alua::tocfunc<usec>()},
                {"msec", alua::tocfunc<msec>()},
                {"sec", alua::tocfunc<sec>()},
                {NULL, NULL}
            };

            alua::regmod(L, "atime", alog_funcs);
        }
    );
}
