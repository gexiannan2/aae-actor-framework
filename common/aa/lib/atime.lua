if not atime then
    atime = {}
end

local nsec = atime.nsec
local usec = atime.usec
local msec = atime.msec
local sec = atime.sec

-- 获取纳秒时间戳
-- @return number
function atime.nsec()
    -- 为便于阅，读函数说明和二次封装在同一个文件
    -- 第二次就不会进入这里了
    atime["nsec"] = nsec
    return nsec()
end

-- 获取微秒时间戳
-- @return number
function atime.usec()
    atime["usec"] = usec
    return usec()
end

-- 获取毫秒时间戳
-- @return number
function atime.msec()
    atime["msec"] = msec
    return msec()
end

-- 获取秒时间戳
-- @return number
function atime.sec()
    atime["sec"] = sec
    return sec()
end

return atime
