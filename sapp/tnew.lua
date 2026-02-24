
-- 只能在这里创建线程

local maindir = runargs.maindir
-- 创建线程：线程名称、线程入口(Lua模块名)、作业超时告警阈值
local t = athd.newthread("tworker", maindir.."tworker.lua", 50)
alog.info("线程作业示例：")
-- 压入线程作业任务
athd.pushtjob(
    -- 线程对象
    t,
    -- 作业名称，用于超时告警日志
    "thread_job",
    -- 把本线程(虚拟机)的Lua函数抛到目标线程执行
    function (p1, p2, p3)
        -- 作业工作(在这里没有闭包数据，只能访问目标线程的资源)
        alog.info(p1.."-在做作业呢", p2, p3);
        -- 返回作业结果给回执函数
        return p1, p2+1, p3
    end,
    -- 在本线程执行的回执函数
    function (p1, p2, p3)
        -- 打印返回值信息
        alog.info(p1.."-做完作业了", p2, p3);
    end,
    "小明", -- p1，传给作业函数的参数
    10000,  -- p2，传给作业函数的参数
    true    -- p3，传给作业函数的参数
)

-- 创建线程池：线程名称、线程入口(Lua模块名)、线程数量、作业超时告警阈值
local p = athd.newpool("tpool", maindir.."tpool.lua", 3, 0)

alog.info("轮询调度作业示例：")
-- -- 压入作业任务函数到线程池，并在回执函数接收作业结果
athd.pushpjob(
    p,
    "pool_job",        
    function (p1, p2, p3) -- 在作业线程执行作业函数
        alog.info(p1.."轮训调度-在做作业呢", p2, p3);
        return p1, p2+1, p3
    end,
    function (p1, p2, p3) -- 在本线程执行回执函数
        alog.info(p1.."轮训调度-做完作业了", p2, p3);
    end,
    "小明", -- p1
    20000,  -- p2
    true    -- p3
)
athd.pushpjob(
    p,
    "pool_job",        
    function (p1, p2, p3) -- 在作业线程执行作业函数
        alog.info(p1.."轮训调度-在做作业呢", p2, p3);
        return p1, p2+1, p3
    end,
    function (p1, p2, p3) -- 在本线程执行回执函数
        alog.info(p1.."轮训调度-做完作业了", p2, p3);
    end,
    "小明", -- p1
    30000,  -- p2
    true    -- p3
)

alog.info("一致性调度作业示例：")
athd.pushpjobby(
    1001, -- 一致性ID：相同ID在同一个线程执行
    p,
    "pool_job",        
    function (p1, p2, p3) -- 在作业线程执行作业函数
        alog.info(p1.."-在做作业呢", p2, p3);
        return p1, p2+1, p3
    end,
    function (p1, p2, p3) -- 在本线程执行回执函数
        alog.info(p1.."-做完作业了", p2, p3);
    end,
    "小明",
    40000,
    true
)
athd.pushpjobby(
    1002, -- 一致性ID：相同ID在同一个线程执行
    p,
    "pool_job",        
    function (p1, p2, p3) -- 在作业线程执行作业函数
        alog.info(p1.."-在做作业呢", p2, p3);
        return p1, p2+1, p3
    end,
    function (p1, p2, p3) -- 在本线程执行回执函数
        alog.info(p1.."-做完作业了", p2, p3);
    end,
    "小东",
    50000,
    true
)
athd.pushpjobby(
    1001, -- 一致性ID：相同ID在同一个线程执行
    p,
    "pool_job",        
    function (p1, p2, p3) -- 在作业线程执行作业函数
        alog.info(p1.."-在做作业呢", p2, p3);
        return p1, p2+1, p3
    end,
    function (p1, p2, p3) -- 在本线程执行回执函数
        alog.info(p1.."-做完作业了", p2, p3);
    end,
    "小明",
    60000,
    true
)
athd.pushpjobby(
    1002, -- 一致性ID：相同ID在同一个线程执行
    p,
    "pool_job",        
    function (p1, p2, p3) -- 在作业线程执行作业函数
        alog.info(p1.."-在做作业呢", p2, p3);
        return p1, p2+1, p3
    end,
    function (p1, p2, p3) -- 在本线程执行回执函数
        alog.info(p1.."-做完作业了", p2, p3);
    end,
    "小东",
    70000,
    true
)
