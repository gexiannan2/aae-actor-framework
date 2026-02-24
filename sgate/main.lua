local function get_my_dir()
    local source = debug.getinfo(1, 'S').source
    source = source:sub(2)  -- 去掉开头的 "@"
    
    -- 先统一处理反斜杠和正斜杠
    source = source:gsub('\\', '/')  -- Windows 路径转 Unix 风格
    
    -- 提取目录部分（现在统一用正斜杠匹配）
    local dir = source:match('(.*/)') or ''
    
    return dir
end

local maindir = get_my_dir()
if not maindir or maindir == "" then
    print("没有找到mian文件所在目录")
end

package.path = maindir.."../common/app/?.lua"

local apprun = require("app")

-- 以下请根据实际需求修改
-- 服务进程名称
local svcname = "svcgate"

-- ?.lua文件根路径（相对maindir）
local pathroots = "../common;script"

-- ?.so文件根路径（相对maindir）
local cpathroots = "../common;script"


apprun(svcname, maindir, pathroots, cpathroots)
