/********************************************************
 *
 * AA分布式引擎（C++）
 * 
 * 消息路由模块
 * 
 * author: ygluu
 * 
 * 2025 国庆
 * 
 *******************************************************/

#pragma once

#include <cstdint>
#include <string>

#include "aid.h"
#include "aos.h"
#include "athd.h"
#include "atype.h"

extern "C"
{
    struct ref_lobj
    {
        int idx_;        // lua引用索引
        void* obj_;      // lua对象指针
    };
    
    struct lobj
    {
        int idx_;        // lua引用索引
        void* obj_;      // lua对象指针
    };

    using ahar_hand = void(*)(std::uint64_t cid, std::uint64_t mid, const atype::astr& name, const atype::abuf& mb);
}

AA_API std::uint64_t ahar_regmsg(atype::astr name, int type);
AA_API void ahar_listen(atype::astr name, ahar_hand handler, void* data = nullptr, athd::thread* t = nullptr);

AA_API void ahar_enterct(std::uint64_t oid, ref_lobj lo);
AA_API void ahar_leavect(std::uint64_t oid);

AA_API void ahar_send(std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_send_cross(std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_sendto(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_sendto_cross(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb);

AA_API void ahar_call(std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_call_cross(std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_callto(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_callto_cross(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb);

AA_API void ahar_svcbc(std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_svcbc_cross(std::uint64_t mid, const atype::abuf& mb);
AA_API void ahar_gatebc(std::uint64_t mid, const atype::abuf& mb);

AA_API void ahar_groupadd(std::uint64_t gid, std::uint64_t mid);
AA_API void ahar_groupdel(std::uint64_t gid, std::uint64_t mid);
AA_API void ahar_groupclear(std::uint64_t gid);
AA_API void ahar_groupsend(std::uint64_t gid, std::uint64_t mid, atype::abuf mb);

namespace ahar
{
    inline std::uint64_t regmsg(atype::astr name, int type)
    {
        return ahar_regmsg(name, type);
    }

    inline void listen(atype::astr name, ahar_hand handler, void* data = nullptr, athd::thread* t = nullptr)
    {
        ahar_listen(name, handler, data, t);
    }

    inline void enterct(std::uint64_t oid, ref_lobj lo)
    {
        ahar_enterct(oid, lo);
    }

    inline void leavect(std::uint64_t oid)
    {
        ahar_leavect(oid);
    }

    inline void send(std::uint64_t mid, const atype::abuf& mb)
    {
        ahar_send(mid, mb);
    }

    inline void send_cross(std::uint64_t mid, const atype::abuf& mb)
    {
        ahar_send_cross(mid, mb);
    }

    inline void send(const std::string_view& mname, const atype::abuf& mb)
    {
        ahar_send(aid::crc32(mname), mb);
    }    

    inline void send_cross(const std::string_view& mname, const atype::abuf& mb)
    {
        ahar_send_cross(aid::crc32(mname), mb);
    }

    inline void sendto(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb)
    {
        ahar_sendto(rid, mid, mb);
    }

    inline void sendto_cross(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb)
    {
        ahar_sendto_cross(rid, mid, mb);
    }

    inline void sendto(std::uint64_t rid, const std::string_view& mname, const atype::abuf& mb)
    {
        ahar_sendto(rid, aid::crc32(mname), mb);
    }

    inline void sendto_cross(std::uint64_t rid, const std::string_view& mname, const atype::abuf& mb)
    {
        ahar_sendto_cross(rid, aid::crc32(mname), mb);
    }

    inline void svcbc(std::uint64_t mid, const atype::abuf& mb)
    {
        ahar_svcbc(mid, mb);
    }

    inline void svcbc_cross(std::uint64_t mid, const atype::abuf& mb)
    {
        ahar_svcbc_cross(mid, mb);
    }

    inline void svcbc(const std::string_view& mname, const atype::abuf& mb)
    {
        ahar_svcbc(aid::crc32(mname), mb);
    }

    inline void svcbc_cross(const std::string_view& mname, const atype::abuf& mb)
    {
        ahar_svcbc_cross(aid::crc32(mname), mb);
    }

    inline void gatebc(std::uint64_t mid, const atype::abuf& mb)
    {
        ahar_gatebc(mid, mb);
    }

    inline void gatebc(const std::string_view& mname, const atype::abuf& mb)
    {
        ahar_gatebc(aid::crc32(mname), mb);
    }

    inline void groupadd(std::uint64_t gid, std::uint64_t mid)
    {
        ahar_groupadd(gid, mid);
    }

    inline void groupadd(std::uint64_t gid, const std::string_view& mname)
    {
        ahar_groupadd(gid, aid::crc32(mname));
    }

    inline void groupdel(std::uint64_t gid, std::uint64_t mid)
    {
        ahar_groupadd(gid, mid);
    }

    inline void groupdel(std::uint64_t gid, const std::string_view& mname)
    {
        ahar_groupadd(gid, aid::crc32(mname));
    }

    inline void groupclear(std::uint64_t gid)
    {
        ahar_groupclear(gid);
    }

    inline void groupsend(std::uint64_t gid, std::uint64_t mid, atype::abuf mb)
    {
        ahar_groupsend(gid, mid, mb);
    }

    inline void groupsend(std::uint64_t gid, const std::string_view& mname, atype::abuf mb)
    {
        ahar_groupsend(gid, aid::crc32(mname), mb);
    }

}