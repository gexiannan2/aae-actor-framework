require("aa")

alog.info(athd.getctname(), "start...")

mod = {}

-- 服务间消息，前缀smsg_
function mod.smsg_ready()
    -- 在这里初始化模块（系统默认消息）
    alog.info("smsg_ready")
end

-- 服务间消息，前缀smsg_
function mod.smsg_user_info_req(msg)
    -- 这里处理用户登录
end

-- 客户端消息，前缀cmsg_
function mod.cmsg_user_login_req(msg)
end
