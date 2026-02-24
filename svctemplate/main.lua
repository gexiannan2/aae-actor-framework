local function get_my_dir()
    local source = debug.getinfo(1, 'S').source
    -- 去掉开头的 “@” 符号
    source = source:sub(2)
    -- 提取目录部分
    local dir = source:match('(.*/)') or ''   -- *nix 分隔符
    -- 如果在 Windows 上跑，也兼容反斜杠
    dir = dir:gsub('\\', '/')
    return dir
end

local maindir = get_my_dir()
package.path = maindir.."../common/app/?.lua"
local apprun = require("app")

-- 以下请根据实际需求修改
-- 服务进程名称
local svcname = "svcname"

-- ?.lua文件根路径（相对maindir）
local pathroots = "../common;script"

-- ?.so文件根路径（相对maindir）
local cpathroots = "../common;script"


apprun(svcname, maindir, pathroots, cpathroots)
