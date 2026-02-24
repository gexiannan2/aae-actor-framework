
if not aid then
    aid = {}
end

local gen = aid.gen
local crc32 = aid.crc32

-- 生成64位ID
function aid.gen()
    -- 为便于阅，读函数说明和二次封装在同一个文件
    -- 第二次就不会进入这里了
    aid["gen"] = gen
    return gen()
end

-- 计算字符串的crc32值
-- @param s string
function aid.crc32(s)
    aid["crc32"] = crc32
    return crc32(s)
end
