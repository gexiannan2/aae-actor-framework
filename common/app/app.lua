local function apprun(svcname, maindir, pathroots, cpathroots)
    local com_dir = maindir.."../common/"
    local aa_dir = com_dir.."aa/lib/"
    local mp_dir = com_dir.."cmsgpack/"

    print(svcname)
    print(maindir)
    print(com_dir)
    print(aa_dir)
    print(mp_dir)
    print(pathroots)
    print(cpathroots)

    print("LMain 开始", maindir)
    -- 设置路径以便能加载aa和cmsgpack
    package.path = maindir.."?.lua;"..aa_dir.."?.lua"
    if iswin then
        package.cpath = aa_dir.."?.dll;"..mp_dir.."?.dll"
    else
        package.cpath = aa_dir.."?.so;"..mp_dir.."?.so"
    end
    print("path: ", package.path)
    print("cpath: ", package.cpath)

    require("aa")

    -- 设置主目录
    runargs.set("svcname", svcname)
    runargs.set("maindir", maindir)
    runargs.set("pathroots", pathroots)
    runargs.set("cpathroots", cpathroots)
    -- 必须在require("aa")之后执行，从runargs.txt加载运行参数，
    -- 包括lua脚本根目录和服务名等
    runargs.load(maindir.."runargs.txt")
    -- 根据runargs.txt重置path/cpath
    alua.resetpaths()

    local logdir = maindir.."../log"
    local svcname = runargs.svcname
    local isdebug = runargs.isdebug or true
    if not svcname then
        alog.error("文件runargs.txt未设置服务名：svcname")
        return
    end
    -- 日志输出到屏幕
    alog.print_screen(isdebug)
    -- 系统日志也输出到屏幕
    alog.print_syslog(isdebug)
    -- 设置日志输出目录
    alog.setroot(logdir, svcname)

    alog.info("logdir: ", logdir)
    alog.info("svcname: ", svcname)

    -- 创建子线程
    require("tnew")
    -- 加载主线程
    require("tmain")
    -- 进入伺服运行状态
    aapp.run()

    alog.info("LMain 结束")
end

return apprun;