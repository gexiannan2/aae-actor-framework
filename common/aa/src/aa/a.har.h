#include <mutex>
#include <memory>

#include "athd.h"
#include "ahar.h"

namespace ahar
{
    struct mhandler
    {
    public:
        int htype_;
        athd::thread* thread_;
        void* data_;
        void* hand_;
    };

    class minfo
    {
    public:
        int mtype_ = 0; // 消息类型：0：msgpack，1：protobuf
        int stype_ = 0; // 发送类型：0：线程间，1：服务间，2：客户端
        std::uint32_t mid_ = 0;
        std::string mname_;
        std::vector<std::unique_ptr<mhandler>> mhandlers_;
        bool is_tmsg()
        {
            return stype_ == 0;
        }
        bool is_smsg()
        {
            return stype_ == 1;
        }
        bool is_cmsg()
        {
            return stype_ == 2;
        }
    };

    class oinfo
    {
    public:
        int type_; // 0: 进程内的lua对象，1：网关的链接对象，2：在其它进程的对象        
        athd::thread* thread_;
        std::uint64_t idx_;
        void* obj_;
    };

    class mdata
    {
    public:
        std::recursive_mutex mtx_;
        std::vector<std::unique_ptr<minfo>> minfos_;
        std::unordered_map<std::uint32_t, minfo*> minfo_by_id_;
        std::unordered_map<std::string_view, minfo*> minfo_by_name_;
        std::unordered_map<std::uint64_t, std::unique_ptr<oinfo>> oinfo_by_id_;
    };
}