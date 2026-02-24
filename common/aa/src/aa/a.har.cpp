#include "ahcpp.h"

#include "aid.h"
#include "alog.h"
#include "alua.h"

#include "a.har.h"


namespace ahar
{
    mdata* get_mdata()
    {
        static mdata md_;   // 首次调用时构造，顺序可预测
        return &md_;
    }
    auto _ = get_mdata();

    void listen(atype::astr name, void* handler, void* data, athd::thread* t, int htype)
    {
        auto md = get_mdata();
        std::lock_guard<std::recursive_mutex> lock(md->mtx_);
        
        auto id = aid::crc32(name.data(), name.size());
        auto& minfo_by_id = md->minfo_by_id_;
        ahar::minfo* minfo;
        auto it = minfo_by_id.find(id);
        if (it == minfo_by_id.end())
        {
            id = ahar_regmsg(name, 0);
            it = minfo_by_id.find(id);
            if (it == minfo_by_id.end())
            {
                return;
            }
        }
        if (!t)
        {
            t = athd::getct();
        }

        minfo = it->second;
        minfo->mhandlers_.emplace_back(std::make_unique<mhandler>());
        auto mhand = minfo->mhandlers_.back().get();
        mhand->htype_ = htype;
        mhand->data_ = data;
        mhand->hand_ = handler;
        mhand->thread_ = t;
    }

    static auto har_thd_ = athd::newthread("tharbor");
}

AA_API std::uint64_t ahar_regmsg(atype::astr name, int type)
{
    if (name.empty())
    {
        alua::error("ahar_register：消息名为空");
        return 0;
    }
    
    auto md = ahar::get_mdata();
    std::lock_guard<std::recursive_mutex> lock(md->mtx_);

    auto id = aid::crc32(name.data(), name.size());
    auto& minfo_by_id = md->minfo_by_id_;
    auto& minfo_by_name = md->minfo_by_name_;
    
    auto it = minfo_by_id.find(id);
    if (it != minfo_by_id.end())
    {
        auto minfo = it->second;
        if (!(minfo->mname_ == name))
        {
            alua::error("ahar_register：消息“{}-{}”与“{}-{}”ID碰撞，请改名", name, id, minfo->mname_, minfo->mid_);            
            return 0;
        }        
        minfo->mtype_ = type;

        return id;
    }

    md->minfos_.emplace_back(std::make_unique<ahar::minfo>());
    auto minfo = md->minfos_.back().get();

    minfo->mtype_ = type;        
    minfo->mid_ = id;
    minfo->mname_ = name;
    auto stype = 0;
    if (minfo->mname_.find("cmsg") == 0)
    {
        stype = 2;
    }
    else if (minfo->mname_.find("smsg") == 0)
    {
        stype = 1;
    }

    minfo_by_id[id] = minfo;
    minfo_by_name[name] = minfo;

    return id;
}

AA_API void ahar_listen(atype::astr name, ahar_hand handler, void* data, athd::thread* t)
{
    ahar::listen(name, *(void**)&handler, data, t, 0);
}

AA_API void ahar_enterct(std::uint64_t oid, ref_lobj lo)
{
    auto md = ahar::get_mdata();
    std::lock_guard<std::recursive_mutex> lock(md->mtx_);
    auto oinfo = new ahar::oinfo;
    oinfo->type_ = 0;
    oinfo->thread_ = athd::getct();
    oinfo->idx_ = lo.idx_;
    oinfo->obj_ = lo.obj_;
    md->oinfo_by_id_[oid] = std::unique_ptr<ahar::oinfo>(oinfo);
}

AA_API void ahar_leavect(std::uint64_t oid)
{
    auto md = ahar::get_mdata();
    std::lock_guard<std::recursive_mutex> lock(md->mtx_);
    md->oinfo_by_id_.erase(oid);
}

AA_API void ahar_send(std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_send_cross(std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_sendto(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb)
{
    auto do_send = [=]()
        {
            auto md = ahar::get_mdata();
            std::lock_guard<std::recursive_mutex> lock(md->mtx_);
            auto& minfo_by_id = md->minfo_by_id_;

            athd::thread* dest_thd = nullptr;
            if (rid)
            {
                auto& oinfo_by_id_ = md->oinfo_by_id_;
                auto it = oinfo_by_id_.find(rid);
                if (it != oinfo_by_id_.end())
                {
                    dest_thd = it->second->thread_;
                }
            }
            
            auto it = minfo_by_id.find(mid);
            if (it == minfo_by_id.end())
            {
                return;
            }

            auto mi = it->second;
            for (auto& hinfo : mi->mhandlers_)
            {
                if (dest_thd && dest_thd != hinfo->thread_)
                {
                    continue;
                }

                if (hinfo->htype_ == 1)
                {
                    //(reinterpret_cast<ahar_hand_pb>(hinfo->hand_))(0, nullptr);
                }
                else
                {
                    auto t = hinfo->thread_;
                    t->pushjob(
                        mi->mname_.c_str(),
                        [=, hi=hinfo.get(),mb=std::string(mb)]() -> void*
                        {
                            (reinterpret_cast<ahar_hand>(hi->hand_))(0, mid, mi->mname_, mb);
                            return nullptr;
                        },
                        nullptr
                    );
                }

                if (dest_thd)
                {
                    return;
                }
            }
        };

    ahar::har_thd_->pushjob("ahar_send_buf_to", do_send);
}

AA_API void ahar_sendto_cross(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_call(std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_call_cross(std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_callto(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_callto_cross(std::uint64_t rid, std::uint64_t mid, const atype::abuf& mb)
{
    
}

AA_API void ahar_svcbc(std::uint64_t mid, const atype::abuf& mb)
{
    auto do_send = [=]()
        {
            auto md = ahar::get_mdata();
            std::lock_guard<std::recursive_mutex> lock(md->mtx_);
            auto& minfo_by_id = md->minfo_by_id_;

            athd::thread* dest_thd = nullptr;
            
            auto it = minfo_by_id.find(mid);
            if (it == minfo_by_id.end())
            {
                return;
            }

            auto mi = it->second;
            for (auto& hinfo : mi->mhandlers_)
            {
                if (dest_thd && dest_thd != hinfo->thread_)
                {
                    continue;
                }

                if (hinfo->htype_ == 1)
                {
                    //(reinterpret_cast<ahar_hand_pb>(hinfo->hand_))(0, nullptr);
                }
                else
                {
                    auto t = hinfo->thread_;
                    t->pushjob(
                        mi->mname_.c_str(),
                        [=, hi=hinfo.get(),mb=std::string(mb)]() -> void*
                        {
                            (reinterpret_cast<ahar_hand>(hi->hand_))(0, mid, mi->mname_, mb);
                            return nullptr;
                        },
                        nullptr
                    );
                }

                if (dest_thd)
                {
                    return;
                }
            }
        };

    ahar::har_thd_->pushjob("ahar_svc_broadcast_buf", do_send);
}

AA_API void ahar_svcbc_cross(std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_gatebc(std::uint64_t mid, const atype::abuf& mb)
{

}

AA_API void ahar_groupadd(std::uint64_t gid, std::uint64_t mid)
{

}

AA_API void ahar_groupdel(std::uint64_t gid, std::uint64_t mid)
{

}

AA_API void ahar_groupclear(std::uint64_t gid)
{

}

AA_API void ahar_groupsend(std::uint64_t gid, std::uint64_t mid, atype::abuf mb)
{

}
