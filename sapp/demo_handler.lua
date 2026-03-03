local M = {}

local function build_cost(seed)
    local v = tonumber(seed or 0) or 0
    local acc = 0
    for i = 1, 2048 do
        acc = (acc + v * i) % 104729
    end
    return acc
end

function M.handle_LoginReq(req, ctx)
    local player_id = tonumber(req.player_id or ctx.player_id or 0) or 0
    local token = tostring(req.token or "")
    local cost = build_cost(player_id + #token)

    return {
        player_id = player_id,
        nickname = "player_" .. tostring(player_id),
        result = 0,
        message = "login-ok cost=" .. tostring(cost),
    }, {
        player_id = player_id,
        room_id = 0,
        code = 0,
        trace = "login@" .. tostring(ctx.worker),
    }
end

function M.handle_MatchReq(req, ctx)
    local player_id = tonumber(req.player_id or ctx.player_id or 0) or 0
    local room_id = tonumber(req.room_id or ctx.room_id or 0) or 0
    local mode = tonumber(req.mode or 0) or 0
    local cost = build_cost(player_id + room_id + mode)

    return {
        player_id = player_id,
        room_id = room_id,
        state = 1,
        message = "match-ok mode=" .. tostring(mode) .. " cost=" .. tostring(cost),
    }, {
        player_id = player_id,
        room_id = room_id,
        code = 0,
        trace = "match@" .. tostring(ctx.worker),
    }
end

return M
