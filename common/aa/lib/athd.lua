local mp   = require "cmsgpack"
local id_count = 0
local work_fns = {}
local done_fns = {}

if not athd then
    athd = {}
end

local function load_work_fn(job_name, bytecode)
    local f = work_fns[job_name]
    if f then
        return f
    end
    f = assert(load(bytecode))
    work_fns[job_name] = f
    return f
end

--- @brief 返回job_id
local function save_done_fn(done_fn)
    if not done_fn then
        return 0
    end
    local job_id = id_count + 1
    id_count = job_id
    done_fns[job_id] = done_fn
    return job_id
end

local getmain = athd.getmain
local newthread = athd.newthread
local newpool = athd.newpool
local pushtjob = athd.pushtjob
local pushpjob = athd.pushpjob
local pushpjobby = athd.pushpjobby
local getctname = athd.getctname
local getct = athd.getct
local getctid = athd.getctid
local setjobcapecity = athd.setjobcapecity
local setjobtimeoutlimit = athd.setjobtimeoutlimit

--- @brief 线程/线程池创建
function athd.newthread(thd_name, entry_name, ms)
    return newthread(thd_name, entry_name, ms)
end

function athd.newpool(thd_name, entry_name, thd_num, ms)
    return newpool(thd_name, entry_name, thd_num, ms)
end

--- @brief 任务执行（带返回）
function athd.onwork(job_name, swork_fn, sargs)
    local f   = load_work_fn(job_name, swork_fn)
    local tmp = mp.unpack(sargs)
    local rv  = table.pack(f(table.unpack(tmp, 1, tmp.n or #tmp)))
    return mp.pack(rv)
end

--- @brief 任务回执（无返回）
function athd.ondone(job_id, sargs)
    local done_fn = done_fns[job_id]
    if not done_fn then
        return
    end
    if not sargs then
        done_fn()
        return
    end
    local tmp = mp.unpack(sargs)
    done_fn(table.unpack(tmp, 1, tmp.n or #tmp))
end

--- @brief 将作业任务压入指定线程
--- @param thread 线程对象，new_thread返回值
--- @param job_name 任务名称，用于超时告警日志
--- @param work_fn function 作业函数，在目标线程执行
--- @param done_fn function 回执函数，回到Call所在线程执行
--- @param args 不定长参数传入work_fn
--- @return work_fn的返回的不定长参数值传入传入回执函数done_fn
function athd.pushtjob(thread, job_name, work_fn, done_fn, ...)
    ahar["pushtjob"] = pushtjob
    pushtjob(thread, save_done_fn(done_fn),
        job_name, string.dump(work_fn), mp.pack({...}))
end

--- @brief 将作业任务压入指定线程池（轮询调度）
--- @param pool 线程池对象，new_pool返回值
--- @param job_name 任务名称，用于超时告警日志
--- @param work_fn function 作业函数，在目标线程执行
--- @param done_fn function 回执函数，回到Call所在线程执行
--- @param args 不定长参数传入作业函数work_fn
--- @return work_fn的返回的不定长参数值传入传入回执函数done_fn
function athd.pushpjob(pool, job_name, work_fn, done_fn, ...)
    ahar["pushpjob"] = pushpjob
    pushpjob(pool, save_done_fn(done_fn), 
        job_name, string.dump(work_fn), mp.pack({...}))
end

--- @brief 将作业任务压入指定线程池（一致性调度)
--- @param cid 一致性id/key，同一个cid的作业任务在同一个线程执行
--- @param pool 线程池对象，new_pool返回值
--- @param job_name 任务名称，用于超时告警日志
--- @param work_fn function 作业函数，在目标线程执行
--- @param done_fn function 回执函数，回到Call所在线程执行
--- @param args 不定长参数传入作业函数work_fn
--- @return work_fn的返回的不定长参数值传入传入回执函数done_fn
function athd.pushpjobby(cid, pool, job_name, work_fn, done_fn, ...)
    ahar["pushpjobby"] = pushpjobby
    pushpjobby(cid, pool, save_done_fn(done_fn),
        job_name, string.dump(work_fn), mp.pack({...}))
end

function athd.getmain()
    --- 为便于阅，读函数说明和二次封装在同一个文件
    --- 第二次就不会进入这里了
    ahar["getmain"] = getmain
    return getmain();
end

function athd.getct()
    ahar["getct"] = getct
    return getct();
end

function athd.getctid()
    ahar["getctid"] = getctid
    return getctid();
end

function athd.getctname()
    ahar["getctname"] = getctname
    return getctname();
end

function athd.setjobcapecity(v)
    ahar["setjobcapecity"] = setjobcapecity
    setjobcapecity(v);
end

function athd.setjobtimeoutlimit(tp, v)
    ahar["setjobtimeoutlimit"] = setjobtimeoutlimit
    setjobtimeoutlimit(tp, v);
end
