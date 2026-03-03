require("aa")

alog.info(athd.getctname(), "start...")

mod = {}

-- 服务间消息，前缀smsg_
-- 消息名等于函数名
-- 框架自动遍历模块函数名自动注册消息handler
function mod.smsg_ready()
    alog.info("smsg_ready")

    if sapp_net_service and sapp_net_service.startup then
        sapp_net_service.startup()
        alog.info("sapp listening",
            "tcp=" .. tostring(sapp_net_service.tcp_listener_id),
            "udp=" .. tostring(sapp_net_service.udp_id))
    end
end

-- 服务间消息，前缀smsg_
function mod.smsg_user_info_req(msg)
    -- 这里获取用户信息
end

-- 客户端消息，前缀cmsg_
function mod.cmsg_user_login_req(msg)
    -- 这里处理用户登录
end

-- 用户操作请求
function mod.cmsg_oper_req(msg, user)
    -- 这里处理用户登录
end

function mod.smsg_net_dispatch_done(msg)
    msg = msg or {}
    alog.info("sapp actor notify",
        "thread=" .. tostring(athd.getctname()),
        "notify=" .. tostring(msg.notify_thread or ""),
        "conn=" .. tostring(msg.conn_id or 0),
        "cmd=" .. tostring(msg.cmd or 0),
        "route=" .. tostring(msg.route or ""),
        "route_key=" .. tostring(msg.route_key or 0),
        "worker=" .. tostring(msg.worker or ""),
        "trace=" .. tostring(msg.trace or ""))
end
