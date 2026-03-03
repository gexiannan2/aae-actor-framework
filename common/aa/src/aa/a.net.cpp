#include "ahcpp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "alog.h"
#include "anet.h"

namespace
{
    using clock_type = std::chrono::steady_clock;

    int set_nonblocking(int fd)
    {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0)
        {
            return -1;
        }
        return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    std::string errno_text(int code)
    {
        return std::strerror(code);
    }

    bool to_sockaddr(const anet::endpoint& value, sockaddr_in& addr, std::string& error_text)
    {
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(value.port);

        if (value.host.empty() || value.host == "0.0.0.0" || value.host == "*")
        {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            return true;
        }

        if (::inet_pton(AF_INET, value.host.c_str(), &addr.sin_addr) == 1)
        {
            return true;
        }

        error_text = "invalid IPv4 address: " + value.host;
        return false;
    }

    anet::endpoint from_sockaddr(const sockaddr_storage& storage)
    {
        anet::endpoint ep {};
        if (storage.ss_family != AF_INET)
        {
            ep.host = "unknown";
            return ep;
        }

        char text[INET_ADDRSTRLEN] = {};
        const auto* in = reinterpret_cast<const sockaddr_in*>(&storage);
        if (::inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)) == nullptr)
        {
            ep.host = "unknown";
        }
        else
        {
            ep.host = text;
        }
        ep.port = ntohs(in->sin_port);
        return ep;
    }

    std::vector<std::byte> pack_tcp_frame(std::span<const std::byte> payload)
    {
        std::vector<std::byte> frame(payload.size() + sizeof(std::uint32_t));
        const std::uint32_t be_size = htonl(static_cast<std::uint32_t>(payload.size()));
        std::memcpy(frame.data(), &be_size, sizeof(be_size));
        if (!payload.empty())
        {
            std::memcpy(frame.data() + sizeof(be_size), payload.data(), payload.size());
        }
        return frame;
    }
}

namespace anet::pvt
{
    struct pending_write
    {
        std::vector<std::byte> buffer;
        std::size_t sent = 0;
        endpoint remote {};
        bool has_remote = false;
    };

    struct channel
    {
        socket_id id = 0;
        int fd = -1;
        socket_kind kind = socket_kind::tcp_connection;
        endpoint peer {};
        bool connecting = false;
        bool closing = false;
        std::vector<std::byte> read_buffer;
        std::deque<pending_write> write_queue;
        clock_type::time_point last_active = clock_type::now();
        clock_type::time_point connect_deadline = clock_type::time_point::max();
    };

    class reactor_impl final
    {
    public:
        explicit reactor_impl(reactor_options options)
            : options_(std::move(options))
        {
            if (options_.max_events <= 0)
            {
                options_.max_events = 256;
            }
            if (options_.epoll_wait_ms < 0)
            {
                options_.epoll_wait_ms = 0;
            }
            if (options_.read_chunk_size == 0)
            {
                options_.read_chunk_size = 64 * 1024;
            }
            if (options_.max_tcp_packet_size < 4)
            {
                options_.max_tcp_packet_size = 4;
            }
        }

        ~reactor_impl()
        {
            stop();
        }

        bool start()
        {
            bool expected = false;
            if (!running_.compare_exchange_strong(expected, true))
            {
                return true;
            }

            stop_requested_.store(false);
            epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
            if (epoll_fd_ < 0)
            {
                set_last_error_from_errno("epoll_create1");
                running_.store(false);
                return false;
            }

            wake_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (wake_fd_ < 0)
            {
                set_last_error_from_errno("eventfd");
                ::close(epoll_fd_);
                epoll_fd_ = -1;
                running_.store(false);
                return false;
            }

            epoll_event ev {};
            ev.events = EPOLLIN;
            ev.data.fd = wake_fd_;
            if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wake_fd_, &ev) != 0)
            {
                set_last_error_from_errno("epoll_ctl add wakefd");
                ::close(wake_fd_);
                ::close(epoll_fd_);
                wake_fd_ = -1;
                epoll_fd_ = -1;
                running_.store(false);
                return false;
            }

            loop_thread_ = std::thread([this] { loop(); });
            dispatch(event {.type = event_type::started, .kind = socket_kind::tcp_connection});
            return true;
        }

        void stop()
        {
            if (!running_.load())
            {
                return;
            }

            stop_requested_.store(true);
            wake();

            if (loop_thread_.joinable())
            {
                loop_thread_.join();
            }
        }

        bool is_running() const
        {
            return running_.load();
        }

        void set_handler(event_handler handler)
        {
            std::lock_guard<std::mutex> lock(handler_mtx_);
            handler_ = std::move(handler);
        }

        void bind_actor_thread(athd::thread* actor_thread, std::string job_name)
        {
            std::lock_guard<std::mutex> lock(handler_mtx_);
            actor_thread_ = actor_thread;
            actor_job_name_ = std::move(job_name);
        }

        socket_id listen_tcp(const endpoint& local, int backlog)
        {
            if (!running_.load())
            {
                set_last_error("reactor is not running");
                return 0;
            }

            sockaddr_in addr {};
            std::string error_text;
            if (!to_sockaddr(local, addr, error_text))
            {
                set_last_error(std::move(error_text));
                return 0;
            }

            const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
            if (fd < 0)
            {
                set_last_error_from_errno("socket listen");
                return 0;
            }

            if (set_nonblocking(fd) != 0)
            {
                set_last_error_from_errno("fcntl nonblock listen");
                ::close(fd);
                return 0;
            }

            int one = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));

            if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
            {
                set_last_error_from_errno("bind listen");
                ::close(fd);
                return 0;
            }

            if (::listen(fd, backlog) != 0)
            {
                set_last_error_from_errno("listen");
                ::close(fd);
                return 0;
            }

            auto entry = std::make_shared<channel>();
            entry->id = next_id_.fetch_add(1);
            entry->fd = fd;
            entry->kind = socket_kind::tcp_listener;
            entry->peer = local;

            if (!add_channel(entry))
            {
                ::close(fd);
                return 0;
            }

            wake();
            return entry->id;
        }

        socket_id connect_tcp(const endpoint& remote)
        {
            if (!running_.load())
            {
                set_last_error("reactor is not running");
                return 0;
            }

            sockaddr_in addr {};
            std::string error_text;
            if (!to_sockaddr(remote, addr, error_text))
            {
                set_last_error(std::move(error_text));
                return 0;
            }

            const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
            if (fd < 0)
            {
                set_last_error_from_errno("socket connect");
                return 0;
            }

            if (set_nonblocking(fd) != 0)
            {
                set_last_error_from_errno("fcntl nonblock connect");
                ::close(fd);
                return 0;
            }

            auto entry = std::make_shared<channel>();
            entry->id = next_id_.fetch_add(1);
            entry->fd = fd;
            entry->kind = socket_kind::tcp_connection;
            entry->peer = remote;
            entry->last_active = clock_type::now();

            const int ret = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            if (ret == 0)
            {
                entry->connecting = false;
            }
            else if (errno == EINPROGRESS)
            {
                entry->connecting = true;
                entry->connect_deadline = clock_type::now() + options_.connect_timeout;
            }
            else
            {
                set_last_error_from_errno("connect");
                ::close(fd);
                return 0;
            }

            if (!add_channel(entry))
            {
                ::close(fd);
                return 0;
            }

            if (!entry->connecting)
            {
                dispatch(event {
                    .type = event_type::connected,
                    .kind = entry->kind,
                    .id = entry->id,
                    .peer = entry->peer
                });
            }

            wake();
            return entry->id;
        }

        socket_id bind_udp(const endpoint& local)
        {
            if (!running_.load())
            {
                set_last_error("reactor is not running");
                return 0;
            }

            sockaddr_in addr {};
            std::string error_text;
            if (!to_sockaddr(local, addr, error_text))
            {
                set_last_error(std::move(error_text));
                return 0;
            }

            const int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
            if (fd < 0)
            {
                set_last_error_from_errno("socket udp");
                return 0;
            }

            if (set_nonblocking(fd) != 0)
            {
                set_last_error_from_errno("fcntl nonblock udp");
                ::close(fd);
                return 0;
            }

            int one = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));

            if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
            {
                set_last_error_from_errno("bind udp");
                ::close(fd);
                return 0;
            }

            auto entry = std::make_shared<channel>();
            entry->id = next_id_.fetch_add(1);
            entry->fd = fd;
            entry->kind = socket_kind::udp_socket;
            entry->peer = local;

            if (!add_channel(entry))
            {
                ::close(fd);
                return 0;
            }

            wake();
            return entry->id;
        }

        bool send_packet(socket_id id, std::span<const std::byte> payload)
        {
            std::shared_ptr<channel> entry;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = channels_by_id_.find(id);
                if (it == channels_by_id_.end())
                {
                    set_last_error("socket id not found");
                    return false;
                }

                entry = it->second;
                if (entry->kind == socket_kind::tcp_listener)
                {
                    set_last_error("cannot send on tcp listener");
                    return false;
                }

                pending_write item;
                if (entry->kind == socket_kind::tcp_connection)
                {
                    if (payload.size() > options_.max_tcp_packet_size)
                    {
                        set_last_error("tcp packet exceeds max_tcp_packet_size");
                        return false;
                    }
                    item.buffer = pack_tcp_frame(payload);
                }
                else
                {
                    item.buffer.assign(payload.begin(), payload.end());
                }

                entry->write_queue.push_back(std::move(item));
                if (!update_interest_locked(entry))
                {
                    return false;
                }
            }

            wake();
            return true;
        }

        bool send_to(socket_id id, const endpoint& remote, std::span<const std::byte> payload)
        {
            sockaddr_in addr {};
            std::string error_text;
            if (!to_sockaddr(remote, addr, error_text))
            {
                set_last_error(std::move(error_text));
                return false;
            }

            std::lock_guard<std::mutex> lock(mtx_);
            auto it = channels_by_id_.find(id);
            if (it == channels_by_id_.end())
            {
                set_last_error("socket id not found");
                return false;
            }

            auto& entry = it->second;
            if (entry->kind != socket_kind::udp_socket)
            {
                set_last_error("send_to is only valid for udp sockets");
                return false;
            }

            pending_write item;
            item.buffer.assign(payload.begin(), payload.end());
            item.remote = remote;
            item.has_remote = true;
            entry->write_queue.push_back(std::move(item));

            if (!update_interest_locked(entry))
            {
                return false;
            }

            wake();
            return true;
        }

        void close(socket_id id, bool graceful)
        {
            std::optional<event> outgoing;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = channels_by_id_.find(id);
                if (it == channels_by_id_.end())
                {
                    return;
                }

                auto entry = it->second;
                if (graceful && !entry->write_queue.empty())
                {
                    entry->closing = true;
                    update_interest_locked(entry);
                    wake();
                    return;
                }
                outgoing = remove_channel_locked(entry, event_type::closed, 0, "");
            }

            if (outgoing.has_value())
            {
                dispatch(std::move(*outgoing));
            }
        }

        std::string last_error() const
        {
            std::lock_guard<std::mutex> lock(error_mtx_);
            return last_error_;
        }

    private:
        bool add_channel(const std::shared_ptr<channel>& entry)
        {
            std::lock_guard<std::mutex> lock(mtx_);

            epoll_event ev {};
            ev.events = desired_events(*entry);
            ev.data.fd = entry->fd;
            if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, entry->fd, &ev) != 0)
            {
                set_last_error_from_errno("epoll_ctl add channel");
                return false;
            }

            channels_by_id_[entry->id] = entry;
            channels_by_fd_[entry->fd] = entry;
            return true;
        }

        std::uint32_t desired_events(const channel& entry) const
        {
            std::uint32_t events = EPOLLERR | EPOLLHUP | EPOLLRDHUP;

            if (entry.kind == socket_kind::tcp_listener || entry.kind == socket_kind::udp_socket || !entry.connecting)
            {
                events |= EPOLLIN;
            }

            if (entry.connecting || !entry.write_queue.empty())
            {
                events |= EPOLLOUT;
            }

            return events;
        }

        bool update_interest_locked(const std::shared_ptr<channel>& entry)
        {
            epoll_event ev {};
            ev.events = desired_events(*entry);
            ev.data.fd = entry->fd;
            if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, entry->fd, &ev) != 0)
            {
                set_last_error_from_errno("epoll_ctl mod channel");
                return false;
            }
            return true;
        }

        std::optional<event> remove_channel_locked(const std::shared_ptr<channel>& entry,
                                                   event_type type,
                                                   int error_code,
                                                   std::string error_text)
        {
            if (entry->fd >= 0)
            {
                ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, entry->fd, nullptr);
                ::close(entry->fd);
            }

            channels_by_fd_.erase(entry->fd);
            channels_by_id_.erase(entry->id);
            entry->fd = -1;

            event out;
            out.type = type;
            out.kind = entry->kind;
            out.id = entry->id;
            out.peer = entry->peer;
            out.error_code = error_code;
            out.error_text = std::move(error_text);
            return out;
        }

        void dispatch(event outgoing)
        {
            event_handler handler;
            athd::thread* actor_thread = nullptr;
            std::string actor_job_name;
            {
                std::lock_guard<std::mutex> lock(handler_mtx_);
                handler = handler_;
                actor_thread = actor_thread_;
                actor_job_name = actor_job_name_;
            }

            if (!handler)
            {
                return;
            }

            if (actor_thread != nullptr)
            {
                auto holder = std::make_shared<event>(std::move(outgoing));
                actor_thread->pushjob(actor_job_name.c_str(), [handler, holder] {
                    handler(*holder);
                });
                return;
            }

            handler(outgoing);
        }

        void loop()
        {
            std::vector<epoll_event> events(static_cast<std::size_t>(options_.max_events));

            while (!stop_requested_.load())
            {
                const int n = ::epoll_wait(epoll_fd_, events.data(), options_.max_events, options_.epoll_wait_ms);
                if (n < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    dispatch(event {
                        .type = event_type::error,
                        .kind = socket_kind::tcp_connection,
                        .error_code = errno,
                        .error_text = "epoll_wait failed: " + errno_text(errno)
                    });
                    break;
                }

                for (int i = 0; i < n; ++i)
                {
                    const auto& ev = events[static_cast<std::size_t>(i)];
                    if (ev.data.fd == wake_fd_)
                    {
                        drain_wake_fd();
                        continue;
                    }

                    std::shared_ptr<channel> entry;
                    {
                        std::lock_guard<std::mutex> lock(mtx_);
                        auto it = channels_by_fd_.find(ev.data.fd);
                        if (it != channels_by_fd_.end())
                        {
                            entry = it->second;
                        }
                    }

                    if (!entry)
                    {
                        continue;
                    }

                    handle_io(entry, ev.events);
                }

                if (options_.idle_timeout.count() > 0 || options_.connect_timeout.count() > 0)
                {
                    check_timeouts();
                }
            }

            cleanup();
            running_.store(false);
            dispatch(event {.type = event_type::stopped, .kind = socket_kind::tcp_connection});
        }

        void handle_io(const std::shared_ptr<channel>& entry, std::uint32_t events)
        {
            if (entry->kind == socket_kind::tcp_listener)
            {
                if (events & EPOLLIN)
                {
                    handle_accept(entry);
                }
                if (events & (EPOLLERR | EPOLLHUP))
                {
                    close(entry->id, false);
                }
                return;
            }

            if (entry->connecting && (events & EPOLLOUT))
            {
                if (!finish_connect(entry))
                {
                    return;
                }
            }

            if (!entry->connecting && (events & EPOLLIN))
            {
                if (entry->kind == socket_kind::udp_socket)
                {
                    handle_udp_read(entry);
                }
                else
                {
                    handle_tcp_read(entry);
                }
            }

            if (events & EPOLLOUT)
            {
                handle_write(entry);
            }

            if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                close(entry->id, false);
            }
        }

        void handle_accept(const std::shared_ptr<channel>& listener)
        {
            while (true)
            {
                sockaddr_storage peer_addr {};
                socklen_t peer_len = sizeof(peer_addr);
                const int client_fd = ::accept4(listener->fd,
                                                reinterpret_cast<sockaddr*>(&peer_addr),
                                                &peer_len,
                                                SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (client_fd < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        return;
                    }
                    dispatch(event {
                        .type = event_type::error,
                        .kind = listener->kind,
                        .id = listener->id,
                        .error_code = errno,
                        .error_text = "accept4 failed: " + errno_text(errno)
                    });
                    return;
                }

                auto client = std::make_shared<channel>();
                client->id = next_id_.fetch_add(1);
                client->fd = client_fd;
                client->kind = socket_kind::tcp_connection;
                client->peer = from_sockaddr(peer_addr);
                client->last_active = clock_type::now();

                if (!add_channel(client))
                {
                    ::close(client_fd);
                    return;
                }

                dispatch(event {
                    .type = event_type::accepted,
                    .kind = listener->kind,
                    .id = listener->id,
                    .related_id = client->id,
                    .peer = client->peer
                });
            }
        }

        bool finish_connect(const std::shared_ptr<channel>& entry)
        {
            int error_code = 0;
            socklen_t len = sizeof(error_code);
            if (::getsockopt(entry->fd, SOL_SOCKET, SO_ERROR, &error_code, &len) != 0)
            {
                error_code = errno;
            }

            if (error_code != 0)
            {
                std::optional<event> outgoing;
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    outgoing = remove_channel_locked(entry,
                                                     event_type::error,
                                                     error_code,
                                                     "connect failed: " + errno_text(error_code));
                }
                if (outgoing.has_value())
                {
                    dispatch(std::move(*outgoing));
                }
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(mtx_);
                entry->connecting = false;
                entry->last_active = clock_type::now();
                if (!update_interest_locked(entry))
                {
                    return false;
                }
            }

            dispatch(event {
                .type = event_type::connected,
                .kind = entry->kind,
                .id = entry->id,
                .peer = entry->peer
            });
            return true;
        }

        void handle_tcp_read(const std::shared_ptr<channel>& entry)
        {
            std::vector<std::byte> scratch(options_.read_chunk_size);

            while (true)
            {
                const ssize_t n = ::recv(entry->fd, scratch.data(), scratch.size(), 0);
                if (n > 0)
                {
                    entry->last_active = clock_type::now();
                    entry->read_buffer.insert(entry->read_buffer.end(), scratch.begin(), scratch.begin() + n);
                    if (!drain_tcp_packets(entry))
                    {
                        return;
                    }
                    continue;
                }

                if (n == 0)
                {
                    close(entry->id, false);
                    return;
                }

                if (errno == EINTR)
                {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    return;
                }

                std::optional<event> outgoing;
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    outgoing = remove_channel_locked(entry,
                                                     event_type::error,
                                                     errno,
                                                     "recv failed: " + errno_text(errno));
                }
                if (outgoing.has_value())
                {
                    dispatch(std::move(*outgoing));
                }
                return;
            }
        }

        bool drain_tcp_packets(const std::shared_ptr<channel>& entry)
        {
            std::size_t offset = 0;
            while (entry->read_buffer.size() - offset >= sizeof(std::uint32_t))
            {
                std::uint32_t be_size = 0;
                std::memcpy(&be_size, entry->read_buffer.data() + offset, sizeof(be_size));
                const std::size_t frame_size = ntohl(be_size);

                if (frame_size > options_.max_tcp_packet_size)
                {
                    std::optional<event> outgoing;
                    {
                        std::lock_guard<std::mutex> lock(mtx_);
                        outgoing = remove_channel_locked(entry,
                                                         event_type::error,
                                                         EMSGSIZE,
                                                         "tcp frame exceeds max_tcp_packet_size");
                    }
                    if (outgoing.has_value())
                    {
                        dispatch(std::move(*outgoing));
                    }
                    return false;
                }

                if (entry->read_buffer.size() - offset < sizeof(std::uint32_t) + frame_size)
                {
                    break;
                }

                event outgoing;
                outgoing.type = event_type::packet;
                outgoing.kind = entry->kind;
                outgoing.id = entry->id;
                outgoing.peer = entry->peer;
                outgoing.payload.resize(frame_size);
                if (frame_size > 0)
                {
                    std::memcpy(outgoing.payload.data(),
                                entry->read_buffer.data() + offset + sizeof(std::uint32_t),
                                frame_size);
                }
                dispatch(std::move(outgoing));
                offset += sizeof(std::uint32_t) + frame_size;
            }

            if (offset > 0)
            {
                entry->read_buffer.erase(entry->read_buffer.begin(), entry->read_buffer.begin() + static_cast<std::ptrdiff_t>(offset));
            }
            return true;
        }

        void handle_udp_read(const std::shared_ptr<channel>& entry)
        {
            std::vector<std::byte> scratch(options_.read_chunk_size);

            while (true)
            {
                sockaddr_storage peer_addr {};
                socklen_t peer_len = sizeof(peer_addr);
                const ssize_t n = ::recvfrom(entry->fd,
                                             scratch.data(),
                                             scratch.size(),
                                             0,
                                             reinterpret_cast<sockaddr*>(&peer_addr),
                                             &peer_len);
                if (n > 0)
                {
                    entry->last_active = clock_type::now();

                    event outgoing;
                    outgoing.type = event_type::packet;
                    outgoing.kind = entry->kind;
                    outgoing.id = entry->id;
                    outgoing.peer = from_sockaddr(peer_addr);
                    outgoing.payload.assign(scratch.begin(), scratch.begin() + n);
                    dispatch(std::move(outgoing));
                    continue;
                }

                if (n == 0)
                {
                    return;
                }

                if (errno == EINTR)
                {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    return;
                }

                dispatch(event {
                    .type = event_type::error,
                    .kind = entry->kind,
                    .id = entry->id,
                    .error_code = errno,
                    .error_text = "recvfrom failed: " + errno_text(errno)
                });
                return;
            }
        }

        void handle_write(const std::shared_ptr<channel>& entry)
        {
            while (true)
            {
                std::optional<event> outgoing;
                bool need_return = false;

                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    auto it = channels_by_id_.find(entry->id);
                    if (it == channels_by_id_.end())
                    {
                        return;
                    }

                    if (entry->write_queue.empty())
                    {
                        update_interest_locked(entry);
                        if (entry->closing)
                        {
                            outgoing = remove_channel_locked(entry, event_type::closed, 0, "");
                        }
                        need_return = true;
                    }
                    else
                    {
                        auto& item = entry->write_queue.front();
                        const auto* data = item.buffer.data() + item.sent;
                        const auto remaining = item.buffer.size() - item.sent;
                        ssize_t n = 0;

                        if (entry->kind == socket_kind::udp_socket)
                        {
                            if (item.has_remote)
                            {
                                sockaddr_in addr {};
                                std::string ignore_error;
                                if (!to_sockaddr(item.remote, addr, ignore_error))
                                {
                                    outgoing = remove_channel_locked(entry, event_type::error, EINVAL, ignore_error);
                                    need_return = true;
                                }
                                else
                                {
                                    n = ::sendto(entry->fd,
                                                 data,
                                                 remaining,
                                                 0,
                                                 reinterpret_cast<sockaddr*>(&addr),
                                                 sizeof(addr));
                                }
                            }
                            else
                            {
                                n = ::send(entry->fd, data, remaining, 0);
                            }
                        }
                        else
                        {
                            n = ::send(entry->fd, data, remaining, 0);
                        }

                        if (!need_return)
                        {
                            if (n > 0)
                            {
                                entry->last_active = clock_type::now();
                                item.sent += static_cast<std::size_t>(n);
                                if (item.sent == item.buffer.size())
                                {
                                    entry->write_queue.pop_front();
                                }
                                continue;
                            }

                            if (n < 0 && errno == EINTR)
                            {
                                continue;
                            }

                            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                            {
                                update_interest_locked(entry);
                                need_return = true;
                            }
                            else
                            {
                                outgoing = remove_channel_locked(entry,
                                                                 event_type::error,
                                                                 errno,
                                                                 "send failed: " + errno_text(errno));
                                need_return = true;
                            }
                        }
                    }
                }

                if (outgoing.has_value())
                {
                    dispatch(std::move(*outgoing));
                }

                if (need_return)
                {
                    return;
                }
            }
        }

        void check_timeouts()
        {
            std::vector<std::shared_ptr<channel>> timed_out;
            const auto now = clock_type::now();

            {
                std::lock_guard<std::mutex> lock(mtx_);
                for (const auto& [id, entry] : channels_by_id_)
                {
                    (void)id;
                    if (entry->connecting && options_.connect_timeout.count() > 0 && now >= entry->connect_deadline)
                    {
                        timed_out.push_back(entry);
                        continue;
                    }

                    if (options_.idle_timeout.count() > 0 &&
                        entry->kind != socket_kind::tcp_listener &&
                        now - entry->last_active >= options_.idle_timeout)
                    {
                        timed_out.push_back(entry);
                    }
                }
            }

            for (const auto& entry : timed_out)
            {
                std::optional<event> outgoing;
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    auto it = channels_by_id_.find(entry->id);
                    if (it == channels_by_id_.end())
                    {
                        continue;
                    }
                    outgoing = remove_channel_locked(entry,
                                                     event_type::timeout,
                                                     ETIMEDOUT,
                                                     entry->connecting ? "connect timeout" : "idle timeout");
                }
                if (outgoing.has_value())
                {
                    dispatch(std::move(*outgoing));
                }
            }
        }

        void cleanup()
        {
            std::lock_guard<std::mutex> lock(mtx_);
            for (auto& [fd, entry] : channels_by_fd_)
            {
                (void)entry;
                ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                ::close(fd);
            }
            channels_by_fd_.clear();
            channels_by_id_.clear();

            if (wake_fd_ >= 0)
            {
                ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, wake_fd_, nullptr);
                ::close(wake_fd_);
                wake_fd_ = -1;
            }

            if (epoll_fd_ >= 0)
            {
                ::close(epoll_fd_);
                epoll_fd_ = -1;
            }
        }

        void wake()
        {
            if (wake_fd_ < 0)
            {
                return;
            }

            const std::uint64_t one = 1;
            const auto ret = ::write(wake_fd_, &one, sizeof(one));
            (void)ret;
        }

        void drain_wake_fd()
        {
            while (true)
            {
                std::uint64_t value = 0;
                const auto ret = ::read(wake_fd_, &value, sizeof(value));
                if (ret > 0)
                {
                    continue;
                }
                if (ret < 0 && errno == EINTR)
                {
                    continue;
                }
                return;
            }
        }

        void set_last_error(std::string value)
        {
            std::lock_guard<std::mutex> lock(error_mtx_);
            last_error_ = std::move(value);
        }

        void set_last_error_from_errno(std::string_view action)
        {
            set_last_error(std::string(action) + ": " + errno_text(errno));
        }

    private:
        reactor_options options_;
        int epoll_fd_ = -1;
        int wake_fd_ = -1;
        std::atomic_bool running_ {false};
        std::atomic_bool stop_requested_ {false};
        std::thread loop_thread_;

        mutable std::mutex mtx_;
        std::unordered_map<socket_id, std::shared_ptr<channel>> channels_by_id_;
        std::unordered_map<int, std::shared_ptr<channel>> channels_by_fd_;
        std::atomic_uint64_t next_id_ {1};

        mutable std::mutex handler_mtx_;
        event_handler handler_;
        athd::thread* actor_thread_ = nullptr;
        std::string actor_job_name_ = "net_event";

        mutable std::mutex error_mtx_;
        std::string last_error_;
    };
}

namespace anet
{
    reactor::reactor(reactor_options options)
        : impl_(std::make_unique<pvt::reactor_impl>(std::move(options)))
    {
    }

    reactor::~reactor() = default;

    bool reactor::start()
    {
        return impl_->start();
    }

    void reactor::stop()
    {
        impl_->stop();
    }

    bool reactor::is_running() const
    {
        return impl_->is_running();
    }

    void reactor::set_handler(event_handler handler)
    {
        impl_->set_handler(std::move(handler));
    }

    void reactor::bind_actor_thread(athd::thread* actor_thread, std::string job_name)
    {
        impl_->bind_actor_thread(actor_thread, std::move(job_name));
    }

    socket_id reactor::listen_tcp(const endpoint& local, int backlog)
    {
        return impl_->listen_tcp(local, backlog);
    }

    socket_id reactor::connect_tcp(const endpoint& remote)
    {
        return impl_->connect_tcp(remote);
    }

    socket_id reactor::bind_udp(const endpoint& local)
    {
        return impl_->bind_udp(local);
    }

    bool reactor::send_packet(socket_id id, std::span<const std::byte> payload)
    {
        return impl_->send_packet(id, payload);
    }

    bool reactor::send_to(socket_id id, const endpoint& remote, std::span<const std::byte> payload)
    {
        return impl_->send_to(id, remote, payload);
    }

    void reactor::close(socket_id id, bool graceful)
    {
        impl_->close(id, graceful);
    }

    std::string reactor::last_error() const
    {
        return impl_->last_error();
    }
}
