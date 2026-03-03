#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

#include "athd.h"

namespace anet
{
    using socket_id = std::uint64_t;

    enum class socket_kind
    {
        tcp_listener,
        tcp_connection,
        udp_socket
    };

    enum class event_type
    {
        started,
        accepted,
        connected,
        packet,
        closed,
        error,
        timeout,
        stopped
    };

    struct endpoint
    {
        std::string host = "0.0.0.0";
        std::uint16_t port = 0;
    };

    struct reactor_options
    {
        int max_events = 256;
        int epoll_wait_ms = 10;
        std::size_t read_chunk_size = 64 * 1024;
        std::size_t max_tcp_packet_size = 4 * 1024 * 1024;
        std::chrono::milliseconds connect_timeout {5000};
        std::chrono::milliseconds idle_timeout {0};
    };

    struct event
    {
        event_type type = event_type::error;
        socket_kind kind = socket_kind::tcp_connection;
        socket_id id = 0;
        socket_id related_id = 0;
        endpoint peer {};
        std::vector<std::byte> payload;
        int error_code = 0;
        std::string error_text;
    };

    using event_handler = std::function<void(const event&)>;

    template<typename T>
    concept protobuf_message = requires(T message, const T const_message, void* data, int size)
    {
        { const_message.ByteSizeLong() } -> std::convertible_to<int>;
        { const_message.SerializeToArray(data, size) } -> std::convertible_to<bool>;
        { message.ParseFromArray(data, size) } -> std::convertible_to<bool>;
    };

    namespace pvt
    {
        class reactor_impl;
    }

    class reactor final
    {
    public:
        explicit reactor(reactor_options options = {});
        ~reactor();

        reactor(const reactor&) = delete;
        reactor& operator=(const reactor&) = delete;

        bool start();
        void stop();
        bool is_running() const;

        void set_handler(event_handler handler);
        void bind_actor_thread(athd::thread* actor_thread, std::string job_name = "net_event");

        socket_id listen_tcp(const endpoint& local, int backlog = 1024);
        socket_id connect_tcp(const endpoint& remote);
        socket_id bind_udp(const endpoint& local);

        bool send_packet(socket_id id, std::span<const std::byte> payload);
        bool send_to(socket_id id, const endpoint& remote, std::span<const std::byte> payload);
        void close(socket_id id, bool graceful = true);

        std::string last_error() const;

        template<protobuf_message Message>
        bool send_message(socket_id id, const Message& message)
        {
            auto payload = serialize_message(message);
            if (payload.empty() && message.ByteSizeLong() > 0)
            {
                return false;
            }
            return send_packet(id, payload);
        }

        template<protobuf_message Message>
        bool send_to_message(socket_id id, const endpoint& remote, const Message& message)
        {
            auto payload = serialize_message(message);
            if (payload.empty() && message.ByteSizeLong() > 0)
            {
                return false;
            }
            return send_to(id, remote, payload);
        }

    private:
        std::unique_ptr<pvt::reactor_impl> impl_;
    };

    template<protobuf_message Message>
    inline std::vector<std::byte> serialize_message(const Message& message)
    {
        const int size = static_cast<int>(message.ByteSizeLong());
        if (size < 0)
        {
            return {};
        }

        std::vector<std::byte> payload(static_cast<std::size_t>(size));
        if (size == 0)
        {
            return payload;
        }

        if (!message.SerializeToArray(payload.data(), size))
        {
            return {};
        }
        return payload;
    }

    template<protobuf_message Message>
    inline bool parse_message(std::span<const std::byte> payload, Message& message)
    {
        if (payload.empty())
        {
            return message.ParseFromArray(nullptr, 0);
        }
        return message.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    inline std::string to_string(const endpoint& value)
    {
        return value.host + ":" + std::to_string(value.port);
    }
}
