#include "ahcpp.h"

#include "alog.h"
#include "athd.h"
#include "alua.h"
#include "a.har.h"

namespace
{
    auto mod_name = "ahar";

    inline void on_buf(std::uint64_t cid, std::uint64_t mid, const atype::astr& name, const atype::abuf& mb)
    {
        alua::call(mod_name, "on_buf", mid, mb, cid);
    }

    inline void do_listen(atype::astr name)
    {
        ahar_listen(name, on_buf);
    }

    inline void do_regmsg(atype::astr name, int mtype)
    {
        ahar_regmsg(name, mtype);
    }

    auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            static const luaL_Reg alog_funcs[] = {
                {"regmsg", alua::tocfunc<ahar_regmsg>()},
                {"listen", alua::tocfunc<do_listen>()},
                {"enterct", alua::tocfunc<ahar_enterct>()},
                {"leavect", alua::tocfunc<ahar_leavect>()},
                {"send", alua::tocfunc<ahar_send>()},
                {"send_cross", alua::tocfunc<ahar_send_cross>()},
                {"sendto", alua::tocfunc<ahar_sendto>()},
                {"sendto_cross", alua::tocfunc<ahar_sendto_cross>()},
                {"call", alua::tocfunc<ahar_call>()},
                {"call_cross", alua::tocfunc<ahar_call_cross>()},
                {"callto", alua::tocfunc<ahar_callto>()},
                {"callto_cross", alua::tocfunc<ahar_callto_cross>()},
                {"svcbc", alua::tocfunc<ahar_svcbc>()},
                {"svcbc_cross", alua::tocfunc<ahar_svcbc_cross>()},
                {"gatebc", alua::tocfunc<ahar_gatebc>()},
                {"groupadd", alua::tocfunc<ahar_groupadd>()},
                {"groupdel", alua::tocfunc<ahar_groupdel>()},
                {"groupclear", alua::tocfunc<ahar_groupclear>()},
                {"groupsend", alua::tocfunc<ahar_groupsend>()},
                {NULL, NULL}
            };

            alua::regmod(L, mod_name, alog_funcs);
        }
    );
}
