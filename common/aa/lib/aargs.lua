
if not aargs then
    aargs = {}
end

local newarger = aargs.newarger
local load = aargs.load
local reload = aargs.reload
local set = aargs.set

--- @brief 加载参数
--- @param fname string 绝对路径文件名，可以多次调用加载多个文件
function aargs.load(fname)
end

--- 重载已经加载过的参数文件
function aargs.reload()
end

--- @brief 修改参数
--- @param k string
--- @param v string/bool/number
function aargs.set(k, v)
end

--- @brief 新建参数容器arger，arger方法说明见aargs同名函数
--- @param fname string 绝对路径文件名
function aargs.newarger(fname)
    if not fname or #fname == 0 then
        alog.error("创建arger未指定文件名称")
        return
    end

    local ptr = newarger(fname)
    local arger = {}

    arger.load = function (fname)
        load(arger, ptr, fname)
    end
    arger.reload = function ()
        reload(arger, ptr)
    end
    arger.set = function (k, v)
        arger[k] = v
        set(ptr, k, v)
    end
    if fname then
        arger.load(fname)
    end
    return arger
end

--- @brief 默认运行参数，先创建对象，带mian.lua加载全路径名的同名文件runargs.txt
runargs = aargs.newarger("runargs.txt")
