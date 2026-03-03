require("aa")

alog.info(athd.getctname(), "start...")

mod = {}

function mod.smsg_ready()
    if snetpb_demo and snetpb_demo.startup then
        snetpb_demo.startup()
    end

    if snetpb_demo then
        alog.info("snetpb ready",
            "tcp=" .. tostring(snetpb_demo.tcp_listener_id),
            "udp=" .. tostring(snetpb_demo.udp_id))
    else
        alog.info("snetpb ready")
    end
end

function mod.smsg_net_dispatch_done(msg)
    msg = msg or {}
    alog.info("actor notify",
        "thread=" .. tostring(athd.getctname()),
        "notify=" .. tostring(msg.notify_thread or ""),
        "conn=" .. tostring(msg.conn_id or 0),
        "cmd=" .. tostring(msg.cmd or 0),
        "route=" .. tostring(msg.route or ""),
        "route_key=" .. tostring(msg.route_key or 0),
        "worker=" .. tostring(msg.worker or ""),
        "trace=" .. tostring(msg.trace or ""))
end
