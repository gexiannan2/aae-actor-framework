
if not alog then
    alog = {}
end

local info = alog.info
local debug = alog.debug
local waring = alog.waring
local error = alog.error
local setroot = alog.setroot
local setoutdebug = alog.setoutdebug
local print_screen = alog.print_screen
local print_syslog = alog.print_syslog
local print_stack = alog.print_stack
local addfile = alog.addfile

-- 输出调试日志，默认不输出，参阅setoutdebug
-- @param args 不定长参数，number/string/bool输出原值，其它输出对象地址
function alog.debug(...)
    -- 为便于阅，读函数说明和二次封装在同一个文件
    -- 第二次就不会进入这里了
    alog["debug"] = debug
    debug(...)
end

function alog.info(...)    
    alog["info"] = info
    info(...)
end

function alog.waring(...)
    alog["waring"] = waring
    waring(...)
end

function alog.error(...)
    alog["error"] = error
    error(...)
end

function alog.info_fmt(fmt, ...)
    info(string.format(fmt, ...))
end

function alog.debug_fmt(fmt, ...)
    debug(string.format(fmt, ...))
end

function alog.waring_fmt(fmt, ...)
    waring(string.format(fmt, ...))
end

function alog.error_fmt(fmt, ...)
    error(string.format(fmt, ...))
end

-- 设置日志根目录和服务标志
-- 日志目录：root/svc_flag/..._info.log
-- 如：root/agame/2025-10-05-21_agame_sys_info.log (svc_flag = agame, 最好带ip:port，分布式多实例时agame有多个)
-- @param root string 日志根目录
-- @param svc_flag string 服务标志：服务名+ip:port
function alog.setroot(root, svc_flag)
    setroot(root, svc_flag)
end

function alog.setoutdebug()
    setoutdebug()
end

-- 设置日志输出到屏幕，默认不输出
-- @param v bool true: 输出，false不输出
function alog.print_screen(v)
    print_screen(v)
end

-- 设置系统日志输出到屏幕，默认不输出，前提print_screen设置为true
-- @param v bool true: 输出，false不输出
function alog.print_syslog(v)
    print_syslog(v)
end

-- 设置ERROR日志打印堆栈信息，默认打印
-- @param v bool true: 打印，false不打印
function alog.print_stack(v)
    print_stack(v)
end

-- 增加日志文件
-- 日志文件名：2025-10-05-20_[svc_name]_[v]_info.log
--             2025-10-05-21_agame_sys_info.log (系统日志，v=sys, 默认)
--             2025-10-05-21_agame_lua_info.log (LUA日志，v=lua, 默认)
-- @param v string 日志文件命名
function alog.addfile(v)
    addfile(v)
end
