
-- 只能在这里创建线程

local maindir = runargs.maindir

-- 线程名称
local threadname = "tworker"
-- 线程入口文件(Lua模块名)
local entryfile = maindir.."tworker.lua"
-- 作业超时告警阈值
local jobtimeoutlimit = 20

--local pthread = athd.newthread(threadname, entryfile, jobtimeoutlimit)
