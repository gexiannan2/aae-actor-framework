# AAE Reactor 网络库设计（epoll + TCP/UDP + Protobuf）

## 目标

新增 `anet` 模块，作为 `aa` 动态库中的网络层能力：

- Linux 下基于 `epoll` 的 Reactor 事件循环
- 同时支持 TCP 和 UDP
- TCP 使用 4 字节大端长度前缀做包边界
- UDP 直接按 datagram 处理
- 提供 protobuf 风格消息的序列化/反序列化辅助
- 通过 `athd::thread` 把网络事件投递回现有 actor 线程

当前实现文件：

- `common/aa/include/anet.h`
- `common/aa/src/aa/a.net.cpp`

这是一版可编译的基础骨架，重点是把事件循环、I/O、错误处理、优雅关闭和 actor 集成点先建立起来。

## 架构

### 1. Reactor 主循环

`anet::reactor` 内部维护一个独立线程：

- `epoll_fd` 负责监听所有网络 fd
- `eventfd` 用于跨线程唤醒 `epoll_wait`
- `channels_by_id` / `channels_by_fd` 管理连接和 socket 元数据

事件循环职责：

- 监听新连接
- 处理非阻塞读写
- 组装 TCP 完整包
- 分发 UDP datagram
- 检查连接超时和空闲超时
- 关闭连接并产生 `closed/error/timeout` 事件

### 2. 连接模型

每个 socket 都抽象成一个 channel，分三类：

- `tcp_listener`
- `tcp_connection`
- `udp_socket`

每个 channel 维护：

- fd
- 唯一 `socket_id`
- 远端地址
- TCP 接收缓冲
- 发送队列
- `connecting` / `closing` 状态
- 最近活跃时间

### 3. Actor 集成点

`anet::reactor` 支持：

```cpp
void bind_actor_thread(athd::thread* actor_thread, std::string job_name = "net_event");
```

绑定后，所有网络事件都会通过 `actor_thread->pushjob(...)` 异步回投到 actor 线程，避免网络线程直接执行业务逻辑。

如果不绑定 actor 线程，则直接在 Reactor 线程中调用事件处理函数。

## 核心 API

```cpp
anet::reactor reactor;

reactor.set_handler([](const anet::event& ev) {
    // 处理 accepted / connected / packet / error / timeout / closed
});

reactor.start();

auto listener = reactor.listen_tcp({"0.0.0.0", 9000});
auto udp = reactor.bind_udp({"0.0.0.0", 9001});
auto conn = reactor.connect_tcp({"127.0.0.1", 9000});

reactor.send_packet(conn, bytes);
reactor.send_to(udp, {"127.0.0.1", 9001}, bytes);
reactor.close(conn, true);
reactor.stop();
```

## Protobuf 编解码

`anet.h` 没有直接依赖 protobuf 头文件，而是用模板约束匹配 protobuf 常见接口：

- `ByteSizeLong()`
- `SerializeToArray(...)`
- `ParseFromArray(...)`

这让 `anet` 不需要在基础库层强绑 `libprotobuf`，只要消息类型具备 protobuf 风格接口就能工作。

### 序列化

```cpp
MyProto msg;
msg.set_id(42);

std::vector<std::byte> payload = anet::serialize_message(msg);
reactor.send_message(conn_id, msg);
reactor.send_to_message(udp_id, {"127.0.0.1", 9001}, msg);
```

### 反序列化

```cpp
void on_event(const anet::event& ev)
{
    if (ev.type != anet::event_type::packet)
    {
        return;
    }

    MyProto msg;
    if (!anet::parse_message(ev.payload, msg))
    {
        // 记录坏包
        return;
    }

    // 使用 msg
}
```

### TCP 与 UDP 的差异

- TCP：`send_message()` 自动加 4 字节包长前缀；接收端 `event.payload` 已经是拆包后的完整 protobuf payload
- UDP：`send_to_message()` 直接发一个 datagram；接收端 `event.payload` 就是 datagram 内容

## 与现有 actor 框架集成示例

下面示例把网络事件回投到 `athd` 线程，再通过 `ahar` 继续分发到业务模块。

```cpp
#include "anet.h"
#include "athd.h"
#include "alog.h"

class net_service
{
public:
    void start()
    {
        actor_thread_ = athd::newthread("tnet-logic", [this] {
            alog::info("network actor started");
        });

        reactor_.bind_actor_thread(actor_thread_, "net_event");
        reactor_.set_handler([this](const anet::event& ev) {
            on_network_event(ev);
        });

        if (!reactor_.start())
        {
            alog::error("reactor start failed: {}", reactor_.last_error());
            return;
        }

        tcp_listener_id_ = reactor_.listen_tcp({"0.0.0.0", 7000});
        udp_socket_id_ = reactor_.bind_udp({"0.0.0.0", 7001});
    }

    void stop()
    {
        reactor_.stop();
    }

private:
    void on_network_event(const anet::event& ev)
    {
        switch (ev.type)
        {
        case anet::event_type::accepted:
            alog::info("accepted conn={}, peer={}", ev.related_id, anet::to_string(ev.peer));
            break;

        case anet::event_type::packet:
        {
            MyProto msg;
            if (!anet::parse_message(ev.payload, msg))
            {
                alog::warning("protobuf decode failed, conn={}", ev.id);
                return;
            }

            // 这里可以转成 actor 消息，再交给 ahar / Lua 业务层
            // ahar::send(...);
            break;
        }

        case anet::event_type::error:
            alog::error("net error on {}: {} ({})", ev.id, ev.error_text, ev.error_code);
            break;

        case anet::event_type::timeout:
            alog::warning("net timeout on {}: {}", ev.id, ev.error_text);
            break;

        case anet::event_type::closed:
            alog::info("connection closed: {}", ev.id);
            break;

        default:
            break;
        }
    }

private:
    athd::thread* actor_thread_ = nullptr;
    anet::reactor reactor_;
    anet::socket_id tcp_listener_id_ = 0;
    anet::socket_id udp_socket_id_ = 0;
};
```

## 错误处理策略

当前实现已覆盖的常见错误：

- `socket/bind/listen/connect` 失败
- 非阻塞 `recv/send/recvfrom/sendto` 错误
- `epoll_wait` 失败
- 连接建立超时
- 连接空闲超时
- TCP 包长超过阈值（默认 4MB）
- 非法地址输入

对外体现方式：

- `reactor::last_error()` 返回最近一次同步 API 错误
- 异步运行时错误通过 `anet::event{ type = error }` 上报

## 优雅关闭

```cpp
reactor.close(conn_id, true);
```

含义：

- 如果发送队列为空，立即关闭
- 如果仍有待发数据，先继续 flush，全部写完后再关闭

强制关闭：

```cpp
reactor.close(conn_id, false);
```

## 当前边界

这版是“先接入框架的基础设施”，不是最终形态。当前还没有做的点：

- TLS
- IPv6
- 零拷贝发送（`sendmsg/writev`）
- 多 Reactor / 多核 accept 分摊
- 背压策略和高水位控制

如果后续要继续往生产级推进，建议下一步优先补：

1. 连接级高低水位与背压控制
2. `writev/sendmsg` 批量发送
3. 定长对象池替代 `std::vector` 频繁扩容
4. 多 Reactor / 多核 accept 分摊

## Lua 绑定

`anet` 现在已经接入 `aa` 的默认加载链：

- `common/aa/src/aa/a.net.lua.cpp`
- `common/aa/lib/anet.lua`
- `common/aa/lib/aa.lua` 已追加 `require("anet")`

因此在任意现有服务脚本里，只要先 `require("aa")`，就可以直接使用：

```lua
local reactor = anet.new({
    max_events = 256,
    connect_timeout_ms = 3000,
    idle_timeout_ms = 60000,
})

reactor:on(function (ev)
    if ev.type == "packet" then
        print("recv bytes:", #ev.data)
    elseif ev.type == "error" then
        print("socket error:", ev.error_text)
    end
end)

local ok, err = reactor:start()
assert(ok, err)

local tcp_id = assert(reactor:listen_tcp("0.0.0.0", 9527))
local udp_id = assert(reactor:bind_udp("0.0.0.0", 9528))
```

Lua 方法说明：

- `anet.new(options)`
- `reactor:on(function(ev) ... end)`：注册回调，并自动绑定到当前 `athd` 线程
- `reactor:start()`
- `reactor:stop()`
- `reactor:is_running()`
- `reactor:listen_tcp(host, port[, backlog])`
- `reactor:connect_tcp(host, port)`
- `reactor:bind_udp(host, port)`
- `reactor:send(socket_id, payload_string)`
- `reactor:sendto(socket_id, host, port, payload_string)`
- `reactor:close(socket_id[, graceful])`

## 游戏服分发模板

当前仓库还补了一个可直接复用的 Lua 分发模板：

- `common/app/net_dispatch.lua`

它把“网关线程收包 -> 线程池处理 -> 回到原线程回包”抽成了统一模式：

- 先用 `packet_decode_fn` 解外层包（例如 `NetEnvelope`）
- 按 `cmd` 查找 `routes[cmd]`
- 用 `route_key` 做一致性调度
- worker 线程里再解业务 body、执行 handler、重新组包
- `done_fn` 回到原收包线程后执行通知和发包

内置了几个常用路由键：

- `net_dispatch.route_keys.by_conn`
- `net_dispatch.route_keys.by_player`
- `net_dispatch.route_keys.by_room`
- `net_dispatch.route_keys.by_hash(fn)`

## 示例服务

`snetpb` 展示了一个更贴近游戏服的完整示例：

- 协议定义：`snetpb/proto/demo.proto`
- 生成脚本：`snetpb/gen_proto.sh`
- 生成产物：`snetpb/script/gen/demo_pb.lua`
- 路由与网络接入：`snetpb/tnew.lua`
- 业务处理：`snetpb/script/handlers/demo_handler.lua`
- 外部模拟客户端：`snetpb/tools/mock_client.py`

TCP 采用统一外层包：

- `NetEnvelope.seq`
- `NetEnvelope.cmd`
- `NetEnvelope.player_id`
- `NetEnvelope.room_id`
- `NetEnvelope.body`
- `NetEnvelope.code`
- `NetEnvelope.trace`

当前 demo 路由：

- `1001 -> LoginReq -> LoginRsp`，按 `player_id` 一致性调度
- `2001 -> MatchReq -> MatchRsp`，按 `room_id` 一致性调度

这条链路已经验证过：

1. 网络线程收到 TCP 包，记录 `queued`
2. 业务被投递到 `tnetbiz` 线程池
3. worker 完成后通过 `done_fn` 回到 `tmain`
4. 回到 `tmain` 后先通知当前 actor 模块
5. 随后在 `tmain` 执行 `reactor:send(...)` 发回响应

外部客户端验证命令：

```bash
python3 snetpb/tools/mock_client.py --cmd login --player-id 30001
```

它会真实连到 `127.0.0.1:9527`，发送一个 `LoginReq` 的 `NetEnvelope`，并打印解析后的 `LoginRsp`。
- `reactor:last_error()`

Lua 事件对象字段：

- `ev.type`
- `ev.kind`
- `ev.id`
- `ev.related_id`
- `ev.peer.host`
- `ev.peer.port`
- `ev.data`
- `ev.size`
- `ev.error_code`
- `ev.error_text`

## 示例服务

新增了一个可直接运行的示例服务：

- `snetpb/`

它会在启动时自动：

- 启动 TCP 监听（默认 `9527`）
- 启动 UDP 监听（默认 `9528`）
- TCP 自连并发送一条 protobuf 包
- TCP 回显一条 protobuf 包
- UDP 向自身发送一条 protobuf 包

入口文件：

- `snetpb/main.lua`
- `snetpb/tnew.lua`
- `snetpb/tmain.lua`
- `common/app/net_dispatch.lua`
- `snetpb/script/gen/demo_pb.lua`
- `snetpb/script/handlers/demo_handler.lua`
- `snetpb/proto/demo.proto`

运行：

```bash
cd /mnt/c/work/AA/aae-master
./snetpb/gen_proto.sh
./snetpb/start.sh
```

说明：

- `common/app/net_dispatch.lua` 把“收包线程快速投递 -> 线程池处理 -> 回到原线程回包”的模式抽成了通用模板
- `snetpb/script/handlers/demo_handler.lua` 是业务处理模块示例，真实服务只需要替换这里
- `snetpb/gen_proto.sh` 是当前的 proto 生成入口：若安装了 `protoc` 会先做 schema 校验，再调用 `tools/proto_to_lua.py` 生成 `demo_pb.lua`
- 由于当前环境没有安装 `protoc`，仓库里这次生成走的是“跳过 `protoc` 校验，但仍生成 Lua 产物”的降级路径
