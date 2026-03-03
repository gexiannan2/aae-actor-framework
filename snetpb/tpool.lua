require("aa")
require("demo_pb")
require("demo_handler")

alog.info(athd.getctname(), "start...")

mod = {}

function mod.smsg_ready()
    alog.info(athd.getctname(), "worker ready")
end
