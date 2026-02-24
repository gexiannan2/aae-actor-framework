#pragma once

#include <functional>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace asig
{

using handler_t = std::function<void(int)>;

// 工具
void block_all_signals();
void unblock_all_signals();

// 信号管理类
class sys_signal
{
public:
    explicit sys_signal(handler_t on_signal = {});
    ~sys_signal();
    void wait();
#ifdef _WIN32
    static handler_t g_handler_;
    static BOOL WINAPI console_handler(DWORD);
    static LONG WINAPI seh_handler(EXCEPTION_POINTERS*);
#endif

private:
    handler_t on_signal_{};

#ifdef _WIN32    
    void* done_event_{};   // HANDLE
#else
    std::thread thread_{};
    void listener_loop();
    static void listener_main(sys_signal* self);
#endif
};

}
