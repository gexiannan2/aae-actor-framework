# athd 函数调用对比：C++ vs Lua

## 概述

`athd` 模块的函数被注册到 Lua 中，但这**不意味着只能从 Lua 调用**。这些函数本质上是 C++ 函数，在 C++ 和 Lua 中都可以调用。

---

## 调用方式对比

### 1. athd::getctname() - 获取当前线程名称

**C++ 调用：**
```cpp
#include "athd.h"

void my_function() {
    const char* name = athd::getctname();
    std::cout << "线程名称: " << name << std::endl;
}
```

**Lua 调用：**
```lua
local name = athd.getctname()
print("线程名称:", name)
```

**函数定义：**
```cpp
// athd.h:260-263
namespace athd {
    inline const char* getctname() {
        return athd_gettname(athd_getctid());
    }
}
```

---

### 2. athd_getmain() - 获取主线程

**C++ 调用：**
```cpp
#include "athd.h"

void my_function() {
    void* main_thread = athd_getmain();
    // 或者使用 C++ 封装
    athd::thread* main_thread = athd::getmain();
}
```

**Lua 调用：**
```lua
local main_thread = athd.getmain()
```

**函数定义：**
```cpp
// athd.h (C API)
AA_API void* athd_getmain();

// athd.h:250-253 (C++ 封装)
namespace athd {
    inline thread* getmain() {
        return static_cast<thread*>(athd_getmain());
    }
}
```

---

### 3. athd_getct() - 获取当前线程

**C++ 调用：**
```cpp
#include "athd.h"

void my_function() {
    void* current_thread = athd_getct();
    // 或者使用 C++ 封装
    athd::thread* ct = athd::getct();
}
```

**Lua 调用：**
```lua
local current_thread = athd.getct()
```

**函数定义：**
```cpp
// athd.h (C API)
AA_API void* athd_getct(void);

// athd.h:244-248 (C++ 封装)
namespace athd {
    inline thread* getct() {
        return static_cast<thread*>(athd_getct());
    }
}
```

---

### 4. athd_getctid() - 获取当前线程 ID

**C++ 调用：**
```cpp
#include "athd.h"

void my_function() {
    std::uint64_t tid = athd_getctid();
    std::cout << "线程 ID: " << tid << std::endl;
    
    // 或者使用 C++ 封装
    std::uint64_t tid2 = athd::getctid();
}
```

**Lua 调用：**
```lua
local tid = athd.getctid()
print("线程 ID:", tid)
```

**函数定义：**
```cpp
// athd.h (C API)
AA_API std::uint64_t athd_getctid();

// athd.h:255-258 (C++ 封装)
namespace athd {
    inline std::uint64_t getctid() {
        return athd_getctid();
    }
}
```

---

### 5. new_thread() - 创建新线程

**C++ 调用（推荐使用 C++ 封装）：**
```cpp
#include "athd.h"

void my_function() {
    // C++ 封装方式（推荐）
    athd::thread* worker = athd::newthread(
        "worker1",                           // 线程名称
        []() {                               // 启动回调
            std::cout << "线程启动" << std::endl;
        },
        []() {                               // 停止回调
            std::cout << "线程停止" << std::endl;
        },
        50                                   // 超时阈值（毫秒）
    );
    
    // 向线程推送任务
    worker->pushjob("task1", 
        []() {
            std::cout << "执行任务" << std::endl;
        }
    );
}
```

**Lua 调用：**
```lua
local worker = athd.newthread("worker1", "worker.lua", 50)
athd.pushtjob(worker, 0, "task1", "print('执行任务')", "")
```

**函数定义（Lua 绑定版本）：**
```cpp
// a.thread.lua.cpp:22-39
void* new_thread(const char* thd_name, const char* entry_file, int ms)
{
    auto ret = athd::newthread(thd_name,
                        [ef=std::string(entry_file)]()
                        {
                            alua::newstate();
                            alua::loadfile(ef.c_str());
                        },
                        []()
                        {
                            alua::closestate();
                        },
                        ms);
    auto md = athd::get_mdata();
    std::lock_guard<std::recursive_mutex> lk(md->mtx_);
    md->lua_threads_.push_back(static_cast<athd::thread_impl*>((void*)ret));
    return ret;
}
```

**C++ 原生 API：**
```cpp
// athd.h:224-227
namespace athd {
    inline thread* newthread(
        const char* name,
        pvt::tfunc start = nullptr,
        pvt::tfunc stop = nullptr,
        int ms = 50
    );
}
```

---

### 6. new_pool() - 创建线程池

**C++ 调用（推荐使用 C++ 封装）：**
```cpp
#include "athd.h"

void my_function() {
    // C++ 封装方式（推荐）
    athd::pool* worker_pool = athd::newpool(
        "worker_pool",                       // 线程池名称
        4,                                   // 线程数量
        []() {                               // 启动回调
            std::cout << "线程启动" << std::endl;
        },
        []() {                               // 停止回调
            std::cout << "线程停止" << std::endl;
        },
        50                                   // 超时阈值（毫秒）
    );
    
    // 向线程池推送任务
    worker_pool->pushjob("task1", 
        []() {
            std::cout << "执行任务" << std::endl;
        },
        nullptr,
        0  // consistency_id (0 表示轮询)
    );
}
```

**Lua 调用：**
```lua
local pool = athd.newpool("worker_pool", "worker.lua", 4, 50)
athd.pushpjob(pool, 0, "task1", "print('执行任务')", "")
```

**C++ 原生 API：**
```cpp
// athd.h:325-328
namespace athd {
    inline pool* newpool(
        const char* name,
        int num,
        pvt::tfunc start = nullptr,
        pvt::tfunc stop = nullptr,
        int ms = 50
    );
}
```

---

### 7. athd_setjobtimeoutlimit() - 设置超时限制

**C++ 调用：**
```cpp
#include "athd.h"

void my_function() {
    athd::thread* worker = athd::newthread("worker1");
    
    // 设置超时为 5 秒
    athd_setjobtimeoutlimit(worker, 5000);
    
    // 或者使用 C++ 封装
    athd::setjobtimeoutlimit(worker, 5000);
}
```

**Lua 调用：**
```lua
local worker = athd.newthread("worker1", "worker.lua")
athd.setjobtimeoutlimit(worker, 5000)
```

**函数定义：**
```cpp
// athd.h (C API)
AA_API void athd_setjobtimeoutlimit(void* tp, std::size_t v);

// athd.h:275-278 (C++ 封装)
namespace athd {
    inline void setjobtimeoutlimit(void* tp, std::size_t v) {
        athd_setjobtimeoutlimit(tp, v);
    }
}
```

---

### 8. athd_setjobcapecity() - 设置任务队列容量

**C++ 调用：**
```cpp
#include "athd.h"

void my_function() {
    // 设置全局任务队列容量
    athd_setjobcapecity(10000);
    
    // 或者使用 C++ 封装
    athd::setjobcapecity(10000);
}
```

**Lua 调用：**
```lua
athd.setjobcapecity(10000)
```

**函数定义：**
```cpp
// athd.h (C API)
AA_API void athd_setjobcapecity(std::size_t v);

// athd.h:270-273 (C++ 封装)
namespace athd {
    inline void setjobcapecity(std::size_t v) {
        athd_setjobcapecity(v);
    }
}
```

---

## 特殊情况：Lua 专用函数

注意这三个函数是**专门为 Lua 设计的**，在 C++ 中不建议使用：

### 9. lua_pushtjob() - Lua 专用

这个函数是为 Lua 调用设计的，它会：
1. 从 Lua 栈提取参数
2. 在工作线程中调用 Lua 脚本
3. 将结果返回给 Lua

**在 C++ 中应该使用：**
```cpp
#include "athd.h"

void my_function() {
    athd::thread* worker = athd::newthread("worker1");
    
    // C++ 方式推送任务
    worker->pushjob("my_task",
        []() {  // work 函数
            std::cout << "执行任务" << std::endl;
        },
        []() {  // done 回调
            std::cout << "任务完成" << std::endl;
        }
    );
}
```

### 10. lua_pushpjob() - Lua 专用

**在 C++ 中应该使用：**
```cpp
#include "athd.h"

void my_function() {
    athd::pool* pool = athd::newpool("pool1", 4);
    
    // C++ 方式推送任务
    pool->pushjob("my_task",
        []() {  // work 函数
            std::cout << "执行任务" << std::endl;
        },
        []() {  // done 回调
            std::cout << "任务完成" << std::endl;
        },
        0  // consistency_id
    );
}
```

### 11. lua_pushpjobby() - Lua 专用

**在 C++ 中应该使用：**
```cpp
#include "athd.h"

void my_function() {
    athd::pool* pool = athd::newpool("pool1", 4);
    
    std::uint64_t user_id = 1001;
    
    // C++ 方式推送任务（带一致性 ID）
    pool->pushjob("user_task",
        []() {  // work 函数
            std::cout << "处理用户请求" << std::endl;
        },
        []() {  // done 回调
            std::cout << "请求完成" << std::endl;
        },
        user_id  // 使用用户 ID 作为一致性 ID
    );
}
```

---

## 完整示例

### C++ 端使用示例

```cpp
#include "athd.h"
#include <iostream>

class MyService {
public:
    void init() {
        // 1. 创建工作线程
        worker_ = athd::newthread("my_worker",
            []() {
                std::cout << "工作线程启动，线程名: " 
                          << athd::getctname() << std::endl;
            },
            []() {
                std::cout << "工作线程停止" << std::endl;
            },
            100
        );
        
        // 2. 创建线程池
        pool_ = athd::newpool("my_pool", 4,
            []() {
                std::cout << "线程池线程启动，线程名: " 
                          << athd::getctname() << std::endl;
            },
            []() {
                std::cout << "线程池线程停止" << std::endl;
            },
            100
        );
        
        // 3. 设置超时
        athd::setjobtimeoutlimit(worker_, 5000);
        athd::setjobtimeoutlimit(pool_, 3000);
    }
    
    void process_task(int task_id) {
        // 获取当前线程信息
        std::cout << "当前线程: " << athd::getctname() 
                  << ", ID: " << athd::getctid() << std::endl;
        
        // 推送任务到工作线程
        worker_->pushjob("task",
            [task_id]() {
                std::cout << "执行任务 " << task_id 
                          << " 在线程 " << athd::getctname() << std::endl;
            },
            [task_id]() {
                std::cout << "任务 " << task_id << " 完成" << std::endl;
            }
        );
    }
    
    void process_user_request(uint64_t user_id, const std::string& action) {
        // 推送到线程池，相同 user_id 的请求会在同一线程顺序执行
        pool_->pushjob("user_request",
            [user_id, action]() {
                std::cout << "处理用户 " << user_id 
                          << " 的请求: " << action 
                          << " 在线程 " << athd::getctname() << std::endl;
            },
            [user_id]() {
                std::cout << "用户 " << user_id << " 请求完成" << std::endl;
            },
            user_id  // 一致性 ID
        );
    }
    
private:
    athd::thread* worker_ = nullptr;
    athd::pool* pool_ = nullptr;
};

int main() {
    MyService service;
    service.init();
    
    // 处理一些任务
    service.process_task(1);
    service.process_task(2);
    
    // 处理用户请求
    service.process_user_request(1001, "login");
    service.process_user_request(1001, "update");
    service.process_user_request(1002, "login");
    
    // 等待所有线程停止
    athd::waitstops();
    
    return 0;
}
```

### Lua 端使用示例

```lua
-- 初始化
local worker = athd.newthread("my_worker", "worker.lua", 100)
local pool = athd.newpool("my_pool", "worker.lua", 4, 100)

-- 设置超时
athd.setjobtimeoutlimit(worker, 5000)
athd.setjobtimeoutlimit(pool, 3000)

-- 获取当前线程信息
print("当前线程:", athd.getctname())
print("线程 ID:", athd.getctid())

-- 推送任务到工作线程
athd.pushtjob(worker, 0, "task1", "print('执行任务')", "")

-- 推送任务到线程池
athd.pushpjob(pool, 0, "task2", "print('执行任务')", "")

-- 推送带一致性 ID 的任务
local user_id = 1001
athd.pushpjobby(user_id, pool, 0, "user_task", "print('处理用户请求')", "")
```

---

## 关键区别总结

### 函数类型分类

| 函数 | C++ 直接调用 | Lua 调用 | 说明 |
|------|-------------|----------|------|
| `athd::getctname()` | ✅ 推荐 | ✅ 支持 | 普通 C++ 函数，两端都可用 |
| `athd::getmain()` | ✅ 推荐 | ✅ 支持 | 普通 C++ 函数，两端都可用 |
| `athd::getct()` | ✅ 推荐 | ✅ 支持 | 普通 C++ 函数，两端都可用 |
| `athd::getctid()` | ✅ 推荐 | ✅ 支持 | 普通 C++ 函数，两端都可用 |
| `athd::newthread()` | ✅ 推荐 | ✅ 支持 | C++ 版本用 lambda，Lua 版本用脚本 |
| `athd::newpool()` | ✅ 推荐 | ✅ 支持 | C++ 版本用 lambda，Lua 版本用脚本 |
| `athd::setjobtimeoutlimit()` | ✅ 推荐 | ✅ 支持 | 普通 C++ 函数，两端都可用 |
| `athd::setjobcapecity()` | ✅ 推荐 | ✅ 支持 | 普通 C++ 函数，两端都可用 |
| `thread->pushjob()` | ✅ 推荐 | ⚠️ 用 pushtjob | C++ 用方法，Lua 用全局函数 |
| `pool->pushjob()` | ✅ 推荐 | ⚠️ 用 pushpjob | C++ 用方法，Lua 用全局函数 |

### 设计原则

1. **查询类函数**（getctname, getctid 等）
   - ✅ C++ 和 Lua 完全一致
   - 直接调用即可

2. **管理类函数**（newthread, newpool 等）
   - ✅ C++ 使用 lambda 回调
   - ✅ Lua 使用脚本文件
   - 两者语义相同，实现不同

3. **任务推送函数**（pushjob）
   - ✅ C++ 使用对象方法 + lambda
   - ✅ Lua 使用全局函数 + 代码字符串
   - 完全不同的调用方式

---

## 最佳实践

### 在 C++ 中

```cpp
// ✓ 推荐：使用 C++ API
#include "athd.h"

athd::thread* t = athd::newthread("worker");
const char* name = athd::getctname();
t->pushjob("task", []() { /* work */ });
```

### 在 Lua 中

```lua
-- ✓ 推荐：使用 Lua API
local t = athd.newthread("worker", "worker.lua")
local name = athd.getctname()
athd.pushtjob(t, 0, "task", "-- work code", "")
```

### ✗ 不要混用

```cpp
// ✗ 不要在 C++ 中使用 Lua 专用函数
lua_pushtjob(L);  // 这需要 lua_State*，很麻烦

// ✓ 应该使用
worker->pushjob("task", []() { /* work */ });
```

---

## 总结

**核心要点：**

1. ✅ **所有注册到 Lua 的函数都是普通的 C++ 函数**
2. ✅ **在 C++ 中直接调用，不需要经过 Lua**
3. ✅ **注册到 Lua 只是为了让 Lua 能调用它们**
4. ⚠️ **部分函数（pushjob）有 C++ 和 Lua 两套 API**
5. 🎯 **根据使用场景选择合适的 API**

**记住：注册 ≠ 绑定 ≠ 只能从 Lua 调用！**

