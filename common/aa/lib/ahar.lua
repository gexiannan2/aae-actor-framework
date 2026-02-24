function get_my_dir()
    local source = debug.getinfo(1, 'S').source
    --- 去掉开头的 “@” 符号
    source = source:sub(2)
    --- 提取目录部分
    local dir = source:match('(.*/)') or ''   --- *nix 分隔符
    --- 如果在 Windows 上跑，也兼容反斜杠
    dir = dir:gsub('\\', '/')
    return dir
end

if ahar == nil then
    ahar = {}
end

local pb_encode = function (name, msg)
    return ahar.mp.pack(msg)
end
local pb_decode = function (name, buf)
    return ahar.mp.unpack(buf)
end
local mp = "cmsgpack"
local mp_encode = mp.pack
local mp_decode = mp.unpack

local regmsg = ahar.regmsg
local listen = ahar.listen
local leavect = ahar.leavect
local enterct = ahar.enterct
local send = ahar.send
local send_cross = ahar.send_cross
local sendto = ahar.sendto
local sendto_cross = ahar.sendto_cross
local call = ahar.call
local call_cross = ahar.call_cross
local callto = ahar.callto
local callto_cross = ahar.callto_cross
local svcbc = ahar.svcbc
local svcbc_cross = ahar.svcbc_cross
local gatebc = ahar.gatebc
local groupadd = ahar.groupadd
local groupdel = ahar.groupdel
local groupclear = ahar.groupclear
local groupsend = ahar.groupsend

local function chech_regmsg(name, type)
    local mis = ahar.msg_info_by_name;
    if not mis then
        mis = {}
        ahar.msg_info_by_id = {};
        ahar.msg_info_by_name = mis;
    end
    local mi = mis[name]
    if mi then
        return mi
    end
    local mid = aid.crc32(name)
    local has = ahar.msg_info_by_id[mid]
    if has and has.name ~= name then
        alog.error_fmt("消息“%s-%d”与已存在的“%s-%d”ID碰撞，请改消息名", name, mid, has.name, has.mid)
        return
    end
    mi = {
        mid=mid,
        name=name,
        type=type,
        cbs={}
    }
    mis[name] = mi
    ahar.msg_info_by_id[mid]=mi
    regmsg(name, type)
    return mi
end

local function encode(name, p1, ...)
    local mi = chech_regmsg(name, 2)
    if not mi then
        return nil, nil
    end
    local buf
    if mi.type == 1 then
        buf = pb_encode(name, p1)
    else
        buf = mp_encode(table.pack(p1, ...))
    end
    return mi.mid, buf
end

function ahar.on_buf(mid, buf, sender)
    local mi = ahar.msg_info_by_id[mid]
    if not mi then
        return
    end
    if mi.type == 1 then
        local msg
        if buf then      
            pb_decode(mi.name, buf)
        end
        for _, cb in ipairs(mi.cbs) do
            if sender then
                cb(sender, msg)
            else
                cb(msg)
            end            
        end
    else
        local msg
        if buf and #buf > 0 then
            msg = mp_decode(buf)
        end
        for _, cb in ipairs(mi.cbs) do
            if sender then
                if msg then
                    cb(sender, table.unpack(msg))
                else
                    cb(sender)
                end
            else
                if msg then
                    cb(table.unpack(msg))
                else
                    cb()
                end
            end
        end
    end
end

--- @brief 注册protobuf消息，其它消息无需注册默认使用cmsgpack格式
function ahar.regpb(name)
    chech_regmsg(name, 1)
end

function ahar.listen(name, cb)
    local mi = chech_regmsg(name, 0)
    table.insert(mi.cbs, cb)
    listen(name)
end

--- @brief 对象离开当前线程
function ahar.leavect(oid)
    ahar["leavect"] = leavect
    leavect(oid)
end

--- @brief 对象进入当前线程
function ahar.enterct(oid)
    ahar["enterct"] = enterct
    enterct(oid)
end

--- @brief 发消息，如果有多个进程接收则轮询均衡
---     可以是单个msg对象也可以是不定长参数
---     收发双方协商消息/参数格式
function ahar.send(name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    send(mid, buf)
end

function ahar.send_cross(name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    send_cross(mid, buf)
end

--- @brief 向rid所在线程发消息(参阅enterct)
function ahar.sendto(rid, name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    sendto(rid, mid, buf)
end

function ahar.sendto_cross(rid, name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    sendto_cross(rid, mid, buf)
end

--- @brief 发送call消息，在回调函数(cb)接收返回消息
function ahar.call(name, cb, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    call(cb, mid, buf)
end

function ahar.call_cross(name, cb, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    call_cross(cb, mid, buf)
end

--- @brief 向rid所在线程发送call消息，
---     在回调函数(cb)接收返回消息
function ahar.callto(rid, name, cb, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    callto(0, rid, cb, mid, buf)
end

function ahar.callto_cross(rid, name, cb, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    callto_cross(1, rid, cb, mid, buf)
end

function ahar.svcbc(name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    svcbc(0, false, mid, buf)
end

function ahar.svcbc_cross(name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    svcbc_cross(1, false, mid, buf)
end

function ahar.gatebc(name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    gatebc(0, true, mid, buf)
end

function ahar.groupadd(gid, mid)
    ahar["groupadd"] = groupadd
    groupadd(gid, mid);
end

function ahar.groupdel(gid, mid)
    ahar["groupdel"] = groupdel
    groupdel(gid, mid);
end

function ahar.groupclear(gid)
    ahar["groupclear"] = groupclear
    groupclear(gid);
end

function ahar.groupsend(gid, name, p1, ...)
    local mid, buf = encode(name, p1, ...)
    if not mid then
        return
    end
    groupsend(gid, mid, buf)
end

local function is_listen_name(s)
    local sub = string.sub(s, 1, 4)
    if sub == "smsg" then
        return 1
    end
    if sub == "cmsg" then
        return 1
    end
    return 0
end

--- @brief 由libaa.so调用
function ahar.auto_listen()
    for mn, mod in pairs(_G) do
        if type(mod) == "table" then
            for fname, func in pairs(mod) do
                if type(func) == "function" and is_listen_name(fname) > 0 then
                    --alog.info_fmt("%s.%s", mn, fname)
                    ahar.listen(fname, func);
                end
            end
        end
    end
end