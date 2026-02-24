#include "ahcpp.h"

#include <string>
#include <span>
#include <iostream>

#include "aos.h"
#include "alog.h"
#include "astr.h"
#include "aargs.h"
#include "afile.h"
#include "alua.h"
#include "athd.h"
#include "ahar.h"
#include "aid.h"

#include "a.signal.h"
#include "a.app.h"
#include "a.thread.h"

// 在 Windows 下需要初始化符号处理
#ifdef _WIN32 // 使用 _WIN32 以兼容 32 位和 64 位系统
#include <windows.h>
#include <dbghelp.h>

#pragma comment(lib, "Dbghelp.lib") // 链接 Dbghelp 库

// 动态链接库的入口点函数
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        // 当 DLL 被加载到进程时初始化符号
        athd::setmainready();
        std::cerr << ">>> DllMain: mydll.dll 正在初始化..." << std::endl;
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
        break;
    case DLL_THREAD_ATTACH:
        // 可选：当创建新线程时执行操作
        break;
    case DLL_THREAD_DETACH:
        // 可选：当线程退出时执行操作
        break;
    case DLL_PROCESS_DETACH:
        // 当 DLL 从进程卸载时清理符号
        SymCleanup(GetCurrentProcess());
        std::cerr << ">>> DllMain: mydll.dll 正在卸载..." << std::endl;
        break;
    }
    return TRUE; // 返回 TRUE 表示成功
}
#endif

namespace aapp
{
    mdata* get_mdata()
    {
        static mdata md_;   // 首次调用时构造，顺序可预测
        return &md_;
    }
    auto _ = get_mdata();
}

void cleanup(int sig)
{

}

void sig_thread()
{
    asig::sys_signal sw(cleanup);
    alog::info("进程进入信号等待模式...");
    sw.wait();
}

AA_API void aapp_run()
{
    alog::info("进入伺服运行状态");
    // 自动注册消息handler
    athd::pushjobtoalllua("auto_listen",
        []()
        {
            alua::call("ahar", "auto_listen");
        },
        []()
        {
            if (athd::jobisok())
            {
                ahar::svcbc("smsg_ready", {});
            }
        });
    // 主线程运行
    athd::do_get_main()->exec();
    // 优雅推出
    athd::waitstops();
}
