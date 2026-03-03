#!/usr/bin/env python3
import argparse
import socket
import struct
import sys


CMD_LOGIN_REQ = 1001
CMD_LOGIN_RSP = 1002
CMD_MATCH_REQ = 2001
CMD_MATCH_RSP = 2002


def encode_varint(value: int) -> bytes:
    value = int(value)
    if value < 0:
        raise ValueError("varint only supports non-negative values")

    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            byte |= 0x80
        out.append(byte)
        if not value:
            return bytes(out)


def decode_varint(buf: bytes, idx: int):
    value = 0
    shift = 0

    while True:
        if idx >= len(buf):
            raise ValueError("varint truncated")
        byte = buf[idx]
        idx += 1
        value |= (byte & 0x7F) << shift
        if byte < 0x80:
            return value, idx
        shift += 7
        if shift > 63:
            raise ValueError("varint overflow")


def encode_key(field_no: int, wire_type: int) -> bytes:
    return encode_varint(field_no * 8 + wire_type)


def encode_varint_field(field_no: int, value: int) -> bytes:
    return encode_key(field_no, 0) + encode_varint(value)


def encode_len_field(field_no: int, value: bytes) -> bytes:
    return encode_key(field_no, 2) + encode_varint(len(value)) + value


def parse_fields(buf: bytes):
    idx = 0
    fields = {}

    while idx < len(buf):
        key, idx = decode_varint(buf, idx)
        field_no = key // 8
        wire_type = key % 8

        if wire_type == 0:
            value, idx = decode_varint(buf, idx)
        elif wire_type == 2:
            size, idx = decode_varint(buf, idx)
            end = idx + size
            if end > len(buf):
                raise ValueError("length-delimited field truncated")
            value = buf[idx:end]
            idx = end
        else:
            raise ValueError(f"unsupported wire type: {wire_type}")

        fields[field_no] = value

    return fields


def encode_net_envelope(msg):
    out = []
    if "seq" in msg:
        out.append(encode_varint_field(1, msg["seq"]))
    if "cmd" in msg:
        out.append(encode_varint_field(2, msg["cmd"]))
    if "player_id" in msg:
        out.append(encode_varint_field(3, msg["player_id"]))
    if "room_id" in msg:
        out.append(encode_varint_field(4, msg["room_id"]))
    if "body" in msg:
        out.append(encode_len_field(5, msg["body"]))
    if "code" in msg:
        out.append(encode_varint_field(6, msg["code"]))
    if "trace" in msg:
        out.append(encode_len_field(7, msg["trace"].encode("utf-8")))
    return b"".join(out)


def decode_net_envelope(buf: bytes):
    fields = parse_fields(buf)
    return {
        "seq": int(fields.get(1, 0)),
        "cmd": int(fields.get(2, 0)),
        "player_id": int(fields.get(3, 0)),
        "room_id": int(fields.get(4, 0)),
        "body": bytes(fields.get(5, b"")),
        "code": int(fields.get(6, 0)),
        "trace": bytes(fields.get(7, b"")).decode("utf-8"),
    }


def encode_login_req(msg):
    out = []
    if "player_id" in msg:
        out.append(encode_varint_field(1, msg["player_id"]))
    if "token" in msg:
        out.append(encode_len_field(2, msg["token"].encode("utf-8")))
    if "device" in msg:
        out.append(encode_len_field(3, msg["device"].encode("utf-8")))
    return b"".join(out)


def decode_login_rsp(buf: bytes):
    fields = parse_fields(buf)
    return {
        "player_id": int(fields.get(1, 0)),
        "nickname": bytes(fields.get(2, b"")).decode("utf-8"),
        "result": int(fields.get(3, 0)),
        "message": bytes(fields.get(4, b"")).decode("utf-8"),
    }


def encode_match_req(msg):
    out = []
    if "player_id" in msg:
        out.append(encode_varint_field(1, msg["player_id"]))
    if "room_id" in msg:
        out.append(encode_varint_field(2, msg["room_id"]))
    if "mode" in msg:
        out.append(encode_varint_field(3, msg["mode"]))
    return b"".join(out)


def decode_match_rsp(buf: bytes):
    fields = parse_fields(buf)
    return {
        "player_id": int(fields.get(1, 0)),
        "room_id": int(fields.get(2, 0)),
        "state": int(fields.get(3, 0)),
        "message": bytes(fields.get(4, b"")).decode("utf-8"),
    }


def recv_exact(sock: socket.socket, size: int) -> bytes:
    chunks = []
    remaining = size

    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise RuntimeError("socket closed while receiving")
        chunks.append(chunk)
        remaining -= len(chunk)

    return b"".join(chunks)


def read_frame(sock: socket.socket) -> bytes:
    header = recv_exact(sock, 4)
    (size,) = struct.unpack(">I", header)
    return recv_exact(sock, size)


def send_frame(sock: socket.socket, payload: bytes):
    sock.sendall(struct.pack(">I", len(payload)) + payload)


def build_request(args):
    if args.cmd == "login":
        body = encode_login_req({
            "player_id": args.player_id,
            "token": args.token,
            "device": args.device,
        })
        cmd = CMD_LOGIN_REQ
        room_id = 0
    else:
        body = encode_match_req({
            "player_id": args.player_id,
            "room_id": args.room_id,
            "mode": args.mode,
        })
        cmd = CMD_MATCH_REQ
        room_id = args.room_id

    envelope = encode_net_envelope({
        "seq": args.seq,
        "cmd": cmd,
        "player_id": args.player_id,
        "room_id": room_id,
        "body": body,
        "code": 0,
        "trace": "mock-client",
    })
    return cmd, envelope


def print_response(envelope):
    print(f"response envelope: seq={envelope['seq']} cmd={envelope['cmd']} player_id={envelope['player_id']} room_id={envelope['room_id']} code={envelope['code']} trace={envelope['trace']}")

    if envelope["cmd"] == CMD_LOGIN_RSP:
        rsp = decode_login_rsp(envelope["body"])
        print(f"login rsp: player_id={rsp['player_id']} nickname={rsp['nickname']} result={rsp['result']} message={rsp['message']}")
        return

    if envelope["cmd"] == CMD_MATCH_RSP:
        rsp = decode_match_rsp(envelope["body"])
        print(f"match rsp: player_id={rsp['player_id']} room_id={rsp['room_id']} state={rsp['state']} message={rsp['message']}")
        return

    print(f"unknown response cmd={envelope['cmd']} body_len={len(envelope['body'])}")


def parse_args():
    parser = argparse.ArgumentParser(description="Simple protobuf-like mock client for snetpb")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9527)
    parser.add_argument("--cmd", choices=["login", "match"], default="login")
    parser.add_argument("--seq", type=int, default=9001)
    parser.add_argument("--player-id", type=int, default=30001)
    parser.add_argument("--room-id", type=int, default=8801)
    parser.add_argument("--mode", type=int, default=5)
    parser.add_argument("--token", default="token-30001")
    parser.add_argument("--device", default="python-mock")
    parser.add_argument("--timeout", type=float, default=3.0)
    return parser.parse_args()


def main():
    args = parse_args()
    req_cmd, payload = build_request(args)
    print(f"send request: cmd={req_cmd} host={args.host}:{args.port} bytes={len(payload)}")

    with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
        sock.settimeout(args.timeout)
        send_frame(sock, payload)
        response = read_frame(sock)

    envelope = decode_net_envelope(response)
    print_response(envelope)
    return 0


if __name__ == "__main__":
    sys.exit(main())
