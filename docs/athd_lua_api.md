# athd Lua API 文档

## 概述

`athd` 是 AA 分布式引擎的异步任务调度模块的 Lua 绑定，提供了强大的多线程任务管理能力。该模块允许在 Lua 中创建和管理工作线程、线程池，并向它们分发异步任务。

## 目录

- [模块初始化](#模块初始化)
- [线程管理](#线程管理)
- [线程池管理](#线程池管理)
- [任务调度](#任务调度)
- [配置函数](#配置函数)
- [完整示例](#完整示例)

---

## 模块初始化

在使用 `athd` 模块前，需要先加载模块：

```lua
-- 模块会自动注册，直接使用即可
local thread_name = athd.getctname()
```

---

## 线程管理

### athd.getctname()

获取当前线程的名称。

**返回值：**
- `string` - 当前线程的名称

**示例：**
```lua
local name = athd.getctname()
print("当前线程名称:", name)
```

---

### athd.getct()

获取当前线程对象。

**返回值：**
- `userdata` - 当前线程对象指针

**示例：**
```lua
local current_thread = athd.getct()
```

---

### athd.getctid()

获取当前线程的 ID。

**返回值：**
- `integer` - 当前线程的唯一 ID

**示例：**
```lua
local tid = athd.getctid()
print("当前线程ID:", tid)
```

---

### athd.getmain()

获取主线程对象。

**返回值：**
- `userdata` - 主线程对象指针

**示例：**
```lua
local main_thread = athd.getmain()
```

---

### athd.newthread(name, entry_file, timeout_ms)

创建一个新的工作线程。

**参数：**
- `name` (string) - 线程名称，用于标识和日志
- `entry_file` (string) - 线程启动时加载的 Lua 脚本文件路径
- `timeout_ms` (integer, 可选) - 任务执行超时告警阈值，单位：毫秒，默认 50ms

**返回值：**
- `userdata` - 新创建的线程对象

**示例：**
```lua
-- 创建一个名为 "worker1" 的线程，加载 worker.lua 脚本
local worker_thread = athd.newthread("worker1", "scripts/worker.lua", 100)
```

**注意：**
- 每个线程会有自己独立的 Lua 状态机
- `entry_file` 在线程启动时执行一次，用于初始化线程环境
- 建议在系统初始化阶段创建线程，不支持运行时动态创建销毁

---

## 线程池管理

### athd.newpool(name, entry_file, thread_count, timeout_ms)

创建一个线程池。

**参数：**
- `name` (string) - 线程池名称
- `entry_file` (string) - 每个线程启动时加载的 Lua 脚本路径
- `thread_count` (integer) - 线程池中的线程数量
- `timeout_ms` (integer, 可选) - 任务超时阈值，单位：毫秒，默认 50ms

**返回值：**
- `userdata` - 新创建的线程池对象

**示例：**
```lua
-- 创建包含 4 个线程的线程池
local pool = athd.newpool("db_pool", "scripts/db_worker.lua", 4, 200)
```

---

## 任务调度

### athd.pushtjob(thread, job_id, job_name, func_code, args)

向单个线程推送任务。

**参数：**
- `thread` (userdata) - 目标线程对象
- `job_id` (integer) - 任务 ID，为 0 表示无需回调
- `job_name` (string) - 任务名称，用于日志和调试
- `func_code` (string) - 要执行的 Lua 函数代码或函数名
- `args` (string, 可选) - 传递给任务的参数（序列化后的字符串）

**工作原理：**
1. 任务会在目标线程中调用 `athd.onwork(job_name, func_code, args)`
2. 如果 `job_id` 不为 0，任务完成后会在主线程调用 `athd.ondone(job_id, result)`

**示例：**
```lua
local worker = athd.newthread("worker1", "worker.lua")

-- 推送一个简单任务（无返回值）
athd.pushtjob(worker, 0, "print_task", "print('Hello from worker')", "")

-- 推送需要回调的任务
local job_id = 12345
athd.pushtjob(worker, job_id, "calculate", "return 10 + 20", "")
```

---

### athd.pushpjob(pool, job_id, job_name, func_code, args)

向线程池推送任务（轮询分配）。

**参数：**
- `pool` (userdata) - 线程池对象
- `job_id` (integer) - 任务 ID
- `job_name` (string) - 任务名称
- `func_code` (string) - 要执行的 Lua 代码
- `args` (string, 可选) - 任务参数

**示例：**
```lua
local pool = athd.newpool("worker_pool", "worker.lua", 4)

-- 任务会被分配到池中的某个线程执行
athd.pushpjob(pool, 0, "db_query", "query_user", "{id=123}")
```

---

### athd.pushpjobby(consistency_id, pool, job_id, job_name, func_code, args)

向线程池推送任务，支持一致性 ID（相同 ID 的任务在同一线程顺序执行）。

**参数：**
- `consistency_id` (integer) - 一致性 ID，相同 ID 的任务会被分配到同一线程顺序执行，0 表示轮询
- `pool` (userdata) - 线程池对象
- `job_id` (integer) - 任务 ID
- `job_name` (string) - 任务名称
- `func_code` (string) - 要执行的 Lua 代码
- `args` (string, 可选) - 任务参数

**示例：**
```lua
local pool = athd.newpool("user_pool", "user_handler.lua", 4)

-- 用户 ID 为 1001 的所有请求会在同一个线程顺序处理
local user_id = 1001
athd.pushpjobby(user_id, pool, 0, "user_request", "handle_request", "{action='login'}")
athd.pushpjobby(user_id, pool, 0, "user_request", "handle_request", "{action='update'}")
```

**使用场景：**
- 保证同一用户的操作顺序性
- 避免数据竞争
- 会话状态管理

---

## 配置函数

### athd.setjobcapecity(capacity)

设置任务队列容量（全局配置）。

**参数：**
- `capacity` (integer) - 任务队列最大容量

**示例：**
```lua
-- 设置任务队列容量为 10000
athd.setjobcapecity(10000)
```

---

### athd.setjobtimeoutlimit(thread_or_pool, limit_ms)

设置指定线程或线程池的任务超时限制。

**参数：**
- `thread_or_pool` (userdata) - 线程或线程池对象
- `limit_ms` (integer) - 超时限制，单位：毫秒

**示例：**
```lua
local worker = athd.newthread("worker1", "worker.lua")
-- 设置超时为 5 秒
athd.setjobtimeoutlimit(worker, 5000)

local pool = athd.newpool("pool1", "worker.lua", 4)
-- 设置线程池超时为 3 秒
athd.setjobtimeoutlimit(pool, 3000)
```

---

## 完整示例

### 示例 1：简单的工作线程

**主脚本 (main.lua)：**
```lua
-- 创建工作线程
local worker = athd.newthread("calculator", "worker.lua", 100)

-- 定义任务完成回调
function athd.ondone(job_id, result)
    print("任务完成，ID:", job_id)
    if result then
        print("结果:", result)
    end
end

-- 推送计算任务
local job_id = 1001
athd.pushtjob(worker, job_id, "add_task", 
    "return 100 + 200", 
    "")
```

**工作线程脚本 (worker.lua)：**
```lua
-- 任务处理函数
function athd.onwork(job_name, func_code, args)
    print("执行任务:", job_name)
    
    -- 执行传入的代码
    local func = load(func_code)
    if func then
        local result = func()
        return tostring(result)
    end
    
    return nil
end
```

---

### 示例 2：线程池处理用户请求

**主脚本：**
```lua
-- 创建线程池
local user_pool = athd.newpool("user_handler", "user_worker.lua", 4, 200)

-- 模拟处理多个用户的请求
for user_id = 1, 100 do
    -- 使用用户 ID 作为一致性 ID，保证同一用户的请求顺序处理
    local request = string.format("{user_id=%d, action='login'}", user_id)
    athd.pushpjobby(user_id, user_pool, 0, "user_login", 
        "handle_user_request", 
        request)
end
```

**工作线程脚本 (user_worker.lua)：**
```lua
function athd.onwork(job_name, func_code, args)
    print(athd.getctname(), "处理:", job_name)
    
    -- 解析参数
    local request = load("return " .. args)()
    
    -- 处理用户请求
    if func_code == "handle_user_request" then
        print("用户", request.user_id, "执行", request.action)
        -- 这里执行实际的业务逻辑
        return "success"
    end
    
    return nil
end
```

---

### 示例 3：数据库查询线程池

```lua
-- 创建数据库查询线程池
local db_pool = athd.newpool("db_pool", "db_worker.lua", 8, 500)

-- 设置较大的超时时间（数据库操作可能耗时较长）
athd.setjobtimeoutlimit(db_pool, 10000)

-- 查询回调
function athd.ondone(job_id, result)
    print("查询完成，ID:", job_id, "结果:", result)
end

-- 推送多个查询任务
for i = 1, 100 do
    local job_id = 1000 + i
    local query = string.format("{sql='SELECT * FROM users WHERE id=%d'}", i)
    athd.pushpjob(db_pool, job_id, "db_query", "execute_query", query)
end
```

---

## 最佳实践

### 1. 线程初始化
```lua
-- 在系统启动时创建所有线程
local function init_threads()
    -- 创建专用线程
    local io_thread = athd.newthread("io_handler", "io.lua", 100)
    local net_thread = athd.newthread("net_handler", "net.lua", 50)
    
    -- 创建通用线程池
    local worker_pool = athd.newpool("workers", "worker.lua", 4, 100)
    
    return io_thread, net_thread, worker_pool
end
```

### 2. 错误处理
```lua
function athd.onwork(job_name, func_code, args)
    local success, result = pcall(function()
        local func = load(func_code)
        return func()
    end)
    
    if not success then
        print("任务执行失败:", job_name, result)
        return nil
    end
    
    return tostring(result)
end
```

### 3. 参数序列化
```lua
-- 使用 JSON 或 Lua table 格式传递复杂参数
local json = require("json")

local params = {
    user_id = 123,
    action = "update",
    data = {name = "Alice", age = 30}
}

athd.pushtjob(worker, 0, "update_user", 
    "process_update", 
    json.encode(params))

-- 在 worker 中反序列化
function athd.onwork(job_name, func_code, args)
    local params = json.decode(args)
    -- 处理参数...
end
```

### 4. 避免阻塞
```lua
-- 不要在任务中执行长时间阻塞操作
-- 错误示例：
function athd.onwork(job_name, func_code, args)
    -- 不要这样做！
    os.execute("sleep 10")  -- 阻塞 10 秒
end

-- 正确做法：将长时间任务拆分为多个短任务
```

---

## 注意事项

1. **线程安全**：每个线程有独立的 Lua 状态，线程间不共享全局变量
2. **初始化时机**：建议在系统初始化阶段创建所有线程，运行时不应动态创建/销毁
3. **超时设置**：根据任务特性合理设置超时阈值，避免误报
4. **一致性 ID**：使用一致性 ID 可以保证相关任务的顺序性
5. **回调函数**：`athd.onwork` 和 `athd.ondone` 需要在相应的脚本中实现
6. **资源管理**：确保 `entry_file` 路径正确，文件存在且可访问

---

## 错误排查

### 常见错误

**1. "没有给定线程或线程池"**
```lua
-- 原因：传入的线程对象为 nil
-- 解决：确保线程对象已正确创建
local worker = athd.newthread("worker1", "worker.lua")
if worker then
    athd.pushtjob(worker, 0, "task", "code", "")
end
```

**2. "没有给定作业名称"**
```lua
-- 原因：job_name 参数为 nil 或空
-- 解决：提供有效的任务名称
athd.pushtjob(worker, 0, "my_task", "code", "")  -- ✓
athd.pushtjob(worker, 0, nil, "code", "")         -- ✗
```

**3. "没有给定作业函数"**
```lua
-- 原因：func_code 参数为 nil 或空
-- 解决：提供有效的函数代码
athd.pushtjob(worker, 0, "task", "print('ok')", "")  -- ✓
athd.pushtjob(worker, 0, "task", nil, "")            -- ✗
```

---

## 性能优化建议

1. **合理设置线程数**：根据 CPU 核心数和任务类型选择
   - CPU 密集型：线程数 = CPU 核心数
   - IO 密集型：线程数 = CPU 核心数 × 2~4

2. **使用线程池**：对于大量短任务，使用线程池优于单独的线程

3. **任务粒度**：避免任务过大或过小
   - 过大：影响响应性
   - 过小：调度开销大

4. **批量处理**：将多个小任务合并为一个任务处理

---

## 版本信息

- **模块版本**：1.0
- **作者**：ygluu
- **更新日期**：2025 国庆

---

## 相关文档

- [alua API 文档](./alua_api.md) - Lua C++ 绑定框架
- [athd C++ API 文档](./athd_cpp_api.md) - 底层 C++ 接口
- [AA 分布式引擎概述](./aa_overview.md) - 整体架构说明

---

## 技术支持

如有问题，请参考：
- 源码：`common/aa/src/aa/a.thread.lua.cpp`
- 头文件：`common/aa/include/athd.h`
- 示例代码：`examples/thread_examples/`

