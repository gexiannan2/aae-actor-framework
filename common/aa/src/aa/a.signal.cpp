#include "ahcpp.h"

#include <iostream>
#include <signal.h>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#   include <pthread.h>
#   include <mutex>
#   include <condition_variable>
#   include <thread>
#endif

#include "a.signal.h"

namespace asig
{

// ---------------------------------------------------------------------------
// 公共工具
// ---------------------------------------------------------------------------
#ifndef _WIN32
void block_all_signals()
{
    sigset_t full {};
    sigfillset(&full);
    pthread_sigmask(SIG_BLOCK, &full, nullptr);
}

void unblock_all_signals()
{
    sigset_t full {};
    sigfillset(&full);
    pthread_sigmask(SIG_UNBLOCK, &full, nullptr);
}
#endif

// ---------------------------------------------------------------------------
// Windows 实现
// ---------------------------------------------------------------------------
#ifdef _WIN32

handler_t sys_signal::g_handler_ = nullptr;

BOOL WINAPI sys_signal::console_handler(DWORD ctrl)
{
    int sig = 0;

    switch (ctrl)
    {
        case CTRL_C_EVENT:
        {
            sig = SIGINT;
            break;
        }
        case CTRL_BREAK_EVENT:
        {
            sig = SIGBREAK;
            break;
        }
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        {
            sig = SIGTERM;
            break;
        }
        default:
        {
            return FALSE;
        }
    }

    if (sys_signal::g_handler_)
    {
        sys_signal::g_handler_(sig);
    }

    return TRUE;
}

LONG WINAPI sys_signal::seh_handler(EXCEPTION_POINTERS* info)
{
    const DWORD code = info->ExceptionRecord->ExceptionCode;

    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        {
            if (sys_signal::g_handler_)
            {
                sys_signal::g_handler_(static_cast<int>(code));
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }
        default:
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
}

sys_signal::sys_signal(handler_t on_signal)
    : on_signal_(std::move(on_signal))
{
    g_handler_ = on_signal_;
    done_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (done_event_ == nullptr)
    {
        throw std::runtime_error("CreateEvent failed");
    }

    SetConsoleCtrlHandler(sys_signal::console_handler, TRUE);
    SetUnhandledExceptionFilter(sys_signal::seh_handler);
}

sys_signal::~sys_signal()
{
    SetConsoleCtrlHandler(sys_signal::console_handler, FALSE);
    if (done_event_ != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(done_event_));
    }
}

void sys_signal::wait()
{
    Sleep(INFINITE);
}

// ---------------------------------------------------------------------------
// Linux 实现
// ---------------------------------------------------------------------------
#else

namespace
{
    std::mutex              g_mtx {};
    std::condition_variable g_cv {};
    bool                    g_exit_flag = false;
}

sys_signal::sys_signal(handler_t on_signal)
    : on_signal_(std::move(on_signal))
{
    thread_ = std::thread(listener_main, this);
}

sys_signal::~sys_signal()
{    
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void sys_signal::listener_main(sys_signal* self)
{
    unblock_all_signals();

    sigset_t wait_set {};
    sigemptyset(&wait_set);

    int sigs[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT,
                   SIGSEGV, SIGFPE, SIGILL, SIGBUS };
    for (int s : sigs)
    {
        sigaddset(&wait_set, s);
    }

    while (true)
    {
        int sig = 0;
        const int rc = sigwait(&wait_set, &sig);
        if (rc != 0)
        {
            continue;
        }

        if (self->on_signal_)
        {
            self->on_signal_(sig);
        }

        const bool is_crash = (sig == SIGSEGV || sig == SIGFPE ||
                               sig == SIGILL  || sig == SIGBUS);
        if (is_crash)
        {
            signal(sig, SIG_DFL);
            raise(sig);
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(g_mtx);
                g_exit_flag = true;
            }
            g_cv.notify_one();
            break;
        }
    }
}

void sys_signal::wait()
{
    std::unique_lock<std::mutex> lock(g_mtx);
    g_cv.wait(lock, [] { return g_exit_flag; });
}

#endif // _WIN32

} // namespace xsys