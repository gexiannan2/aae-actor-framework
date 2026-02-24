require("aa")

alog.info(athd.getctname(), "start...")

mod = {}

-- 服务间消息，前缀smsg_
-- 消息名等于函数名
-- 框架自动遍历模块函数名自动注册消息handler
function mod.smsg_ready()
    -- 在这里初始化模块（系统默认消息）
    alog.info("smsg_ready")
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
