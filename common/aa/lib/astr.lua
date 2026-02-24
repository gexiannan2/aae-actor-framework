if not astr then
    astr = {}
end

function astr.tab_to_str(t)
    local fmt, concat = string.format, table.concat
    local type, next, tostring = type, next, tostring

    local buf = {}
    local ref = {}      -- table → id
    local ref_id = 0
    local indent_cache = {}

    local function indent(d)
        local s = indent_cache[d]
        if not s then
            s = "\n" .. ("  "):rep(d)
            indent_cache[d] = s
        end
        buf[#buf + 1] = s
    end

    local function visit(v, d)
        local vt = type(v)
        if vt == "string" then
            buf[#buf + 1] = fmt("%q", v)
        elseif vt == "number" or vt == "boolean" or vt == "nil" then
            buf[#buf + 1] = tostring(v)
        elseif vt == "table" then
            local id = ref[v]
            if id then
                buf[#buf + 1] = "<ref" .. id .. ">"
                return
            end
            ref_id = ref_id + 1
            ref[v] = ref_id

            buf[#buf + 1] = "{"
            local next_d = d + 1
            local n = #v

            -- 数组部分
            for i = 1, n do
                if i ~= 1 then buf[#buf + 1] = "," end
                indent(next_d)
                visit(v[i], next_d)
            end

            -- 字典部分
            local first = (n == 0)
            for k, v2 in next, v do
                local kt = type(k)
                if not (kt == "number" and k >= 1 and k <= n) then
                    if not first then buf[#buf + 1] = "," end
                    indent(next_d)

                    -- key
                    if kt == "string" and k:match("^[_%a][_%w]*$") then
                        buf[#buf + 1] = k .. " = "
                    else
                        buf[#buf + 1] = "["
                        visit(k, next_d)
                        buf[#buf + 1] = "] = "
                    end

                    visit(v2, next_d)
                    first = false
                end
            end

            if not first then indent(d) end
            buf[#buf + 1] = "}"
        else
            buf[#buf + 1] = "<" .. vt .. ">"
        end
    end

    visit(t, 0)
    return concat(buf)
end

function astr.print_tab(t, prefix)
    if prefix then
        print(prefix, astr.tab_to_str(t))
    else
        print(astr.tab_to_str(t))
    end
end

return astr
