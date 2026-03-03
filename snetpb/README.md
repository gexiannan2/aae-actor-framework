# snetpb

`snetpb` 是一个贴近游戏服网关模型的网络 demo，直接接入当前 `main.lua` 服务体系。

它演示了这条完整链路：

- TCP 收包线程只做拆包和路由，不做重业务
- 按 `cmd` 分发到不同 proto handler
- 按 `player_id` / `room_id` 做一致性线程池调度
- 工作线程完成 protobuf 解包、业务处理、组包
- `done_fn` 异步回到原收包线程
- 回到收包线程后先通知当前 actor 模块，再执行回包
- 同时保留一个 UDP `UdpPing` 收发示例

## 协议

`demo.proto` 当前包含：

- `NetEnvelope`：统一 TCP 包头，字段有 `seq/cmd/player_id/room_id/body/code/trace`
- `LoginReq` / `LoginRsp`
- `MatchReq` / `MatchRsp`
- `UdpPing`

路由示例：

- `1001 -> LoginReq`，按 `player_id` 路由
- `2001 -> MatchReq`，按 `room_id` 路由

## 运行

```bash
cd /mnt/c/work/AA/aae-master
./snetpb/gen_proto.sh
./snetpb/start.sh
```

服务启动后会自动做一次进程内自测：

- TCP 自连发送 `LoginReq`
- TCP 自连发送 `MatchReq`
- UDP 自发自收 `UdpPing`

日志里可以看到：

- `tcp server recv queued`
- `tcp biz done`
- `actor notify`
- `tcp client login rsp`
- `tcp client match rsp`

## 外部模拟客户端

另一个终端里可以直接跑：

```bash
python3 snetpb/tools/mock_client.py --cmd login --player-id 30001
```

也可以测匹配请求：

```bash
python3 snetpb/tools/mock_client.py --cmd match --player-id 30001 --room-id 8801 --mode 5
```

它会：

- 手工按 protobuf wire format 编码请求
- 建立真实 TCP 连接
- 发送 4 字节长度前缀 + `NetEnvelope`
- 读取响应并解码打印

## Proto 生成

```bash
./snetpb/gen_proto.sh
```

说明：

- 如果机器上安装了 `protoc`，脚本会先校验 `demo.proto`
- 当前仓库内置 `tools/proto_to_lua.py`，会把 `.proto` 生成成 `snetpb/script/gen/demo_pb.lua`
- 当前这台机器未安装 `protoc`，但仍可用内置生成器完成 Lua 编解码模块生成
