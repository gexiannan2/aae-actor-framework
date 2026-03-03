local function get_my_dir()
    local source = debug.getinfo(1, "S").source
    source = source:sub(2)
    source = source:gsub("\\", "/")
    local dir = source:match("(.*/)") or ""
    return dir
end

local maindir = get_my_dir()
if not maindir or maindir == "" then
    print("没有找到main.lua所在目录")
end

package.path = maindir .. "../common/app/?.lua"

local apprun = require("app")

local svcname = "snetpb"
local pathroots = "../common;script"
local cpathroots = "../common;script"

apprun(svcname, maindir, pathroots, cpathroots)
