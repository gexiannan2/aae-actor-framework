local net_dispatch = {}

net_dispatch.route_keys = {}

function net_dispatch.route_keys.by_conn(_, ev)
    return ev.id
end

function net_dispatch.route_keys.by_player(packet, ev)
    local v = tonumber(packet.player_id or 0) or 0
    if v ~= 0 then
        return v
    end
    return ev.id
end

function net_dispatch.route_keys.by_room(packet, ev)
    local v = tonumber(packet.room_id or 0) or 0
    if v ~= 0 then
        return v
    end
    return ev.id
end

function net_dispatch.route_keys.by_hash(fn)
    assert(type(fn) == "function", "route_keys.by_hash requires a function")
    return function (packet, ev, route)
        local v = tonumber(fn(packet, ev, route) or 0) or 0
        if v ~= 0 then
            return v
        end
        return ev.id
    end
end

local function default_send_error(_, err)
    alog.error("net dispatch send failed", tostring(err))
end

local function default_dispatch_error(_, err)
    alog.error("net dispatch failed", tostring(err))
end

local function default_unknown_packet(ev, packet)
    alog.waring("unknown tcp cmd",
        "conn=" .. tostring(ev.id),
        "cmd=" .. tostring(packet and packet.cmd or 0))
end

local function normalize_key(v, fallback)
    v = tonumber(v or 0) or 0
    if v ~= 0 then
        return v
    end
    return fallback
end

function net_dispatch.new(cfg)
    assert(type(cfg) == "table", "net_dispatch.new requires a config table")
    assert(cfg.reactor, "net_dispatch requires cfg.reactor")
    assert(cfg.pool, "net_dispatch requires cfg.pool")
    assert(type(cfg.packet_codec_module) == "string" and #cfg.packet_codec_module > 0, "net_dispatch requires cfg.packet_codec_module")
    assert(type(cfg.packet_decode_fn) == "string" and #cfg.packet_decode_fn > 0, "net_dispatch requires cfg.packet_decode_fn")
    assert(type(cfg.packet_encode_fn) == "string" and #cfg.packet_encode_fn > 0, "net_dispatch requires cfg.packet_encode_fn")
    assert(type(cfg.routes) == "table", "net_dispatch requires cfg.routes")

    local dispatcher = {
        cfg = cfg,
    }

    function dispatcher:dispatch_tcp_packet(ev)
        local packet_codec = require(self.cfg.packet_codec_module)
        local packet_decode = packet_codec[self.cfg.packet_decode_fn]
        if type(packet_decode) ~= "function" then
            error("packet decode fn not found: " .. tostring(self.cfg.packet_decode_fn))
        end

        local ok, packet = pcall(packet_decode, ev.data)
        if not ok then
            (self.cfg.on_dispatch_error or default_dispatch_error)(ev, packet)
            return false
        end

        local cmd_field = self.cfg.cmd_field or "cmd"
        local cmd = tonumber(packet[cmd_field] or 0) or 0
        local route = self.cfg.routes[cmd]
        if not route then
            (self.cfg.on_unknown_packet or default_unknown_packet)(ev, packet)
            return false
        end

        local route_key_fn = route.route_key or self.cfg.route_key or net_dispatch.route_keys.by_conn
        local route_key = normalize_key(route_key_fn(packet, ev, route), ev.id)

        local on_queued = route.on_queued or self.cfg.on_queued
        if on_queued then
            on_queued(ev, packet, route_key, route)
        end

        local work_ok, work_err = pcall(function ()
            athd.pushpjobby(
                route_key,
                self.cfg.pool,
                route.job_name or self.cfg.job_name or "net_dispatch_job",
                function (conn_id, route_key_arg,
                          seq, cmd_arg, player_id, room_id, code, trace, raw_body,
                          packet_codec_module, packet_encode_name,
                          body_codec_module, body_decode_name, body_encode_name,
                          handler_module, handler_name, response_cmd)
                    local function pick_non_zero(value, fallback)
                        value = tonumber(value or 0) or 0
                        if value ~= 0 then
                            return value
                        end
                        return fallback
                    end

                    local packet_codec_local = require(packet_codec_module)
                    local packet_encode = packet_codec_local[packet_encode_name]
                    if type(packet_encode) ~= "function" then
                        error("packet encode fn not found: " .. tostring(packet_encode_name))
                    end

                    local body_codec = require(body_codec_module)
                    local body_decode = body_codec[body_decode_name]
                    if type(body_decode) ~= "function" then
                        error("body decode fn not found: " .. tostring(body_decode_name))
                    end

                    local body_encode = body_codec[body_encode_name]
                    if type(body_encode) ~= "function" then
                        error("body encode fn not found: " .. tostring(body_encode_name))
                    end

                    local handler_mod = require(handler_module)
                    local handler_fn = handler_mod[handler_name]
                    if type(handler_fn) ~= "function" then
                        error("handler fn not found: " .. tostring(handler_name))
                    end

                    local request = body_decode(raw_body)
                    local response_body, meta = handler_fn(request, {
                        conn_id = conn_id,
                        route_key = route_key_arg,
                        worker = athd.getctname(),
                        seq = seq,
                        cmd = cmd_arg,
                        player_id = player_id,
                        room_id = room_id,
                        code = code,
                        trace = trace,
                        raw_body = raw_body,
                    })

                    if type(meta) ~= "table" then
                        meta = {}
                    end

                    local body_payload = response_body
                    if type(body_payload) ~= "string" then
                        body_payload = body_encode(body_payload or {})
                    end

                    local final_cmd = pick_non_zero(meta.cmd, pick_non_zero(response_cmd, cmd_arg))
                    local final_player_id = meta.player_id
                    if final_player_id == nil then
                        final_player_id = player_id
                    end
                    local final_room_id = meta.room_id
                    if final_room_id == nil then
                        final_room_id = room_id
                    end
                    local final_code = meta.code
                    if final_code == nil then
                        final_code = 0
                    end
                    local final_trace = tostring(meta.trace or "")

                    local packet_payload = packet_encode({
                        seq = meta.seq or seq,
                        cmd = final_cmd,
                        player_id = final_player_id,
                        room_id = final_room_id,
                        code = final_code,
                        trace = final_trace,
                        body = body_payload,
                    })

                    return conn_id, route_key_arg, packet_payload, athd.getctname(), request, {
                        seq = meta.seq or seq,
                        cmd = final_cmd,
                        player_id = final_player_id,
                        room_id = final_room_id,
                        code = final_code,
                        trace = final_trace,
                    }
                end,
                function (conn_id, route_key_arg, payload, worker_name, request, response_meta)
                    local ctx = {
                        conn_id = conn_id,
                        route_key = route_key_arg,
                        payload = payload,
                        worker = worker_name,
                        request = request,
                        response_meta = response_meta or {},
                        route = route,
                        packet = packet,
                    }

                    local on_done = route.on_done or self.cfg.on_done
                    if on_done then
                        on_done(ev, packet, ctx, route)
                    end

                    if route.auto_reply == false or self.cfg.auto_reply == false then
                        return
                    end

                    local sent, send_err = self.cfg.reactor:send(conn_id, payload)
                    if not sent then
                        (route.on_send_error or self.cfg.on_send_error or default_send_error)(ev, send_err, ctx, route)
                    end
                end,
                ev.id,
                route_key,
                packet.seq or 0,
                cmd,
                packet.player_id or 0,
                packet.room_id or 0,
                packet.code or 0,
                tostring(packet.trace or ""),
                packet.body or "",
                self.cfg.packet_codec_module,
                self.cfg.packet_encode_fn,
                route.body_codec_module or self.cfg.packet_codec_module,
                route.decode_fn,
                route.encode_fn,
                route.handler_module,
                route.handler_fn,
                route.response_cmd or cmd
            )
        end)

        if not work_ok then
            (route.on_dispatch_error or self.cfg.on_dispatch_error or default_dispatch_error)(ev, work_err, packet, route)
            return false
        end

        return true
    end

    return dispatcher
end

return net_dispatch
