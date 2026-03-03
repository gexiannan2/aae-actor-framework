local demo_pb = require("demo_pb")
local net_dispatch = require("net_dispatch")

local CMD_LOGIN_REQ = 1001
local CMD_LOGIN_RSP = 1002
local CMD_MATCH_REQ = 2001
local CMD_MATCH_RSP = 2002

local tcp_port = tonumber(runargs.tcp_port or 9527) or 9527
local udp_port = tonumber(runargs.udp_port or 9528) or 9528
local biz_pool_size = tonumber(runargs.biz_pool_size or 4) or 4
local demo_host = "127.0.0.1"

local state = {
    tcp_listener_id = 0,
    tcp_client_id = 0,
    udp_id = 0,
    next_seq = 1,
    client_closed = false,
    client_pending = 0,
    server_conn_ids = {},
}

local maindir = runargs.maindir
state.biz_pool = athd.newpool("tnetbiz", maindir .. "tpool.lua", biz_pool_size, 200)

local reactor = anet.new({
    max_events = 256,
    epoll_wait_ms = 10,
    read_chunk_size = 64 * 1024,
    max_tcp_packet_size = 4 * 1024 * 1024,
    connect_timeout_ms = 3000,
    idle_timeout_ms = 60000,
})

state.reactor = reactor
_G.snetpb_demo = state

local function next_seq()
    local v = state.next_seq
    state.next_seq = v + 1
    return v
end

local function log_send_error(action, err)
    alog.error(action, err or reactor:last_error())
end

local function decode_message(fn_name, payload, label)
    local decode_fn = demo_pb[fn_name]
    if type(decode_fn) ~= "function" then
        alog.error("protobuf decode fn missing", fn_name)
        return nil
    end

    local ok, msg = pcall(decode_fn, payload)
    if not ok then
        alog.error(label or "protobuf decode failed", msg)
        return nil
    end
    return msg
end

local function send_udp_probe()
    local payload = demo_pb.encode_UdpPing({
        seq = next_seq(),
        sent_ms = atime.msec(),
        note = "hello over udp",
    })

    local ok, err = reactor:sendto(state.udp_id, demo_host, udp_port, payload)
    if not ok then
        log_send_error("udp send failed", err)
    end
end

local function send_tcp_packet(cmd, player_id, room_id, body)
    local packet = demo_pb.encode_NetEnvelope({
        seq = next_seq(),
        cmd = cmd,
        player_id = player_id or 0,
        room_id = room_id or 0,
        body = body or "",
        code = 0,
        trace = "lua-self-client",
    })

    local ok, err = reactor:send(state.tcp_client_id, packet)
    if not ok then
        log_send_error("tcp send failed", err)
        return false
    end

    state.client_pending = state.client_pending + 1
    return true
end

local function send_tcp_login_probe()
    return send_tcp_packet(
        CMD_LOGIN_REQ,
        10001,
        0,
        demo_pb.encode_LoginReq({
            player_id = 10001,
            token = "dev-token-10001",
            device = "lua-self-client",
        })
    )
end

local function send_tcp_match_probe()
    return send_tcp_packet(
        CMD_MATCH_REQ,
        10001,
        7001,
        demo_pb.encode_MatchReq({
            player_id = 10001,
            room_id = 7001,
            mode = 3,
        })
    )
end

local function consume_client_response()
    if state.client_pending > 0 then
        state.client_pending = state.client_pending - 1
    end

    if state.client_pending == 0 and not state.client_closed then
        state.client_closed = true
        reactor:close(state.tcp_client_id, true)
    end
end

state.tcp_dispatcher = net_dispatch.new({
    reactor = reactor,
    pool = state.biz_pool,
    job_name = "tcp_proto_request",
    packet_codec_module = "demo_pb",
    packet_decode_fn = "decode_NetEnvelope",
    packet_encode_fn = "encode_NetEnvelope",
    on_queued = function (ev, packet, route_key, route)
        alog.info("tcp server recv queued",
            "conn=" .. tostring(ev.id),
            "cmd=" .. tostring(packet.cmd or 0),
            "route=" .. tostring(route.name or ""),
            "route_key=" .. tostring(route_key),
            "size=" .. tostring(ev.size or #(ev.data or "")))
    end,
    on_done = function (_, packet, ctx, route)
        local response_meta = ctx.response_meta or {}
        local actor_msg = {
            notify_thread = athd.getctname(),
            conn_id = ctx.conn_id,
            cmd = response_meta.cmd or packet.cmd or 0,
            route = route.name or "",
            route_key = ctx.route_key or 0,
            worker = ctx.worker or "",
            trace = response_meta.trace or "",
            player_id = response_meta.player_id or packet.player_id or 0,
            room_id = response_meta.room_id or packet.room_id or 0,
            code = response_meta.code or 0,
        }

        alog.info("tcp biz done",
            "notify=" .. tostring(athd.getctname()),
            "conn=" .. tostring(ctx.conn_id),
            "cmd=" .. tostring(packet.cmd or 0),
            "rsp_cmd=" .. tostring(response_meta.cmd or 0),
            "route=" .. tostring(route.name or ""),
            "route_key=" .. tostring(ctx.route_key or 0),
            "worker=" .. tostring(ctx.worker or ""),
            "trace=" .. tostring(response_meta.trace or ""))

        if type(mod) == "table" and type(mod.smsg_net_dispatch_done) == "function" then
            mod.smsg_net_dispatch_done(actor_msg)
        end
    end,
    on_send_error = function (_, err)
        log_send_error("async tcp send failed", err)
    end,
    on_dispatch_error = function (_, err, packet, route)
        alog.error("dispatch tcp biz failed",
            tostring(err),
            "cmd=" .. tostring(packet and packet.cmd or 0),
            "route=" .. tostring(route and route.name or ""))
    end,
    routes = {
        [CMD_LOGIN_REQ] = {
            name = "login",
            route_key = net_dispatch.route_keys.by_player,
            body_codec_module = "demo_pb",
            decode_fn = "decode_LoginReq",
            encode_fn = "encode_LoginRsp",
            handler_module = "demo_handler",
            handler_fn = "handle_LoginReq",
            response_cmd = CMD_LOGIN_RSP,
        },
        [CMD_MATCH_REQ] = {
            name = "match",
            route_key = net_dispatch.route_keys.by_room,
            body_codec_module = "demo_pb",
            decode_fn = "decode_MatchReq",
            encode_fn = "encode_MatchRsp",
            handler_module = "demo_handler",
            handler_fn = "handle_MatchReq",
            response_cmd = CMD_MATCH_RSP,
        },
    },
})

reactor:on(function (ev)
    if ev.type == "started" then
        alog.info("anet reactor started")
        return
    end

    if ev.type == "accepted" then
        state.server_conn_ids[ev.related_id] = true
        alog.info("tcp accepted",
            "listener=" .. tostring(ev.id),
            "conn=" .. tostring(ev.related_id),
            ev.peer.host .. ":" .. tostring(ev.peer.port))
        return
    end

    if ev.type == "connected" then
        alog.info("tcp connected",
            "conn=" .. tostring(ev.id),
            ev.peer.host .. ":" .. tostring(ev.peer.port))
        if ev.id == state.tcp_client_id then
            send_tcp_login_probe()
            send_tcp_match_probe()
        end
        return
    end

    if ev.type == "packet" then
        if ev.kind == "udp_socket" then
            local msg = decode_message("decode_UdpPing", ev.data, "udp decode failed")
            if not msg then
                return
            end

            alog.info("udp recv",
                "seq=" .. tostring(msg.seq),
                msg.note,
                "from",
                ev.peer.host .. ":" .. tostring(ev.peer.port))
            return
        end

        if state.server_conn_ids[ev.id] then
            state.tcp_dispatcher:dispatch_tcp_packet(ev)
            return
        end

        local envelope = decode_message("decode_NetEnvelope", ev.data, "net envelope decode failed")
        if not envelope then
            return
        end

        if envelope.cmd == CMD_LOGIN_RSP then
            local msg = decode_message("decode_LoginRsp", envelope.body, "login rsp decode failed")
            if not msg then
                return
            end

            alog.info("tcp client login rsp",
                "seq=" .. tostring(envelope.seq),
                "player=" .. tostring(msg.player_id),
                "result=" .. tostring(msg.result),
                "msg=" .. tostring(msg.message),
                "trace=" .. tostring(envelope.trace))
            consume_client_response()
            return
        end

        if envelope.cmd == CMD_MATCH_RSP then
            local msg = decode_message("decode_MatchRsp", envelope.body, "match rsp decode failed")
            if not msg then
                return
            end

            alog.info("tcp client match rsp",
                "seq=" .. tostring(envelope.seq),
                "player=" .. tostring(msg.player_id),
                "room=" .. tostring(msg.room_id),
                "state=" .. tostring(msg.state),
                "msg=" .. tostring(msg.message),
                "trace=" .. tostring(envelope.trace))
            consume_client_response()
            return
        end

        alog.waring("tcp client unknown rsp cmd",
            "cmd=" .. tostring(envelope.cmd))
        return
    end

    if ev.type == "closed" then
        state.server_conn_ids[ev.id] = nil
        alog.info("socket closed",
            "id=" .. tostring(ev.id),
            "kind=" .. tostring(ev.kind))
        return
    end

    if ev.type == "timeout" then
        alog.waring("socket timeout",
            "id=" .. tostring(ev.id),
            ev.error_text)
        return
    end

    if ev.type == "error" then
        alog.error("socket error",
            "id=" .. tostring(ev.id),
            "code=" .. tostring(ev.error_code),
            ev.error_text)
        return
    end

    if ev.type == "stopped" then
        alog.info("anet reactor stopped")
    end
end)

function state.startup()
    if state.started then
        return
    end
    state.started = true

    local ok, err = reactor:start()
    if not ok then
        error("anet.start failed: " .. tostring(err or reactor:last_error()))
    end

    local tcp_listener_id
    tcp_listener_id, err = reactor:listen_tcp("0.0.0.0", tcp_port, 128)
    if not tcp_listener_id then
        error("anet.listen_tcp failed: " .. tostring(err or reactor:last_error()))
    end
    state.tcp_listener_id = tcp_listener_id

    local udp_id
    udp_id, err = reactor:bind_udp("0.0.0.0", udp_port)
    if not udp_id then
        error("anet.bind_udp failed: " .. tostring(err or reactor:last_error()))
    end
    state.udp_id = udp_id

    local tcp_client_id
    tcp_client_id, err = reactor:connect_tcp(demo_host, tcp_port)
    if not tcp_client_id then
        error("anet.connect_tcp failed: " .. tostring(err or reactor:last_error()))
    end
    state.tcp_client_id = tcp_client_id

    send_udp_probe()

    alog.info("snetpb bootstrap done",
        "tcp=" .. tostring(tcp_port),
        "udp=" .. tostring(udp_port),
        "biz_pool=" .. tostring(biz_pool_size))
end
