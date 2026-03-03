#include "ahcpp.h"

#include <lua.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include "alog.h"
#include "alua.h"
#include "anet.h"
#include "athd.h"

namespace
{
    constexpr const char* k_reactor_mt = "aa.anet.reactor";

    struct lua_reactor_state
    {
        std::unique_ptr<anet::reactor> reactor;
        lua_State* owner = nullptr;
        int callback_ref = LUA_NOREF;
        bool closed = false;
    };

    using reactor_handle = std::shared_ptr<lua_reactor_state>;

    reactor_handle* check_handle(lua_State* L, int idx)
    {
        auto* handle = static_cast<reactor_handle*>(luaL_checkudata(L, idx, k_reactor_mt));
        if (!handle || !(*handle))
        {
            luaL_error(L, "invalid anet reactor");
        }
        return handle;
    }

    reactor_handle get_handle(lua_State* L, int idx)
    {
        return *check_handle(L, idx);
    }

    const char* event_name(anet::event_type type)
    {
        switch (type)
        {
        case anet::event_type::started:
            return "started";
        case anet::event_type::accepted:
            return "accepted";
        case anet::event_type::connected:
            return "connected";
        case anet::event_type::packet:
            return "packet";
        case anet::event_type::closed:
            return "closed";
        case anet::event_type::error:
            return "error";
        case anet::event_type::timeout:
            return "timeout";
        case anet::event_type::stopped:
            return "stopped";
        }
        return "unknown";
    }

    const char* kind_name(anet::socket_kind kind)
    {
        switch (kind)
        {
        case anet::socket_kind::tcp_listener:
            return "tcp_listener";
        case anet::socket_kind::tcp_connection:
            return "tcp_connection";
        case anet::socket_kind::udp_socket:
            return "udp_socket";
        }
        return "unknown";
    }

    void push_endpoint(lua_State* L, const anet::endpoint& ep)
    {
        lua_createtable(L, 0, 2);
        lua_pushlstring(L, ep.host.data(), ep.host.size());
        lua_setfield(L, -2, "host");
        lua_pushinteger(L, ep.port);
        lua_setfield(L, -2, "port");
    }

    void push_event(lua_State* L, const anet::event& ev)
    {
        lua_createtable(L, 0, 8);

        lua_pushstring(L, event_name(ev.type));
        lua_setfield(L, -2, "type");

        lua_pushstring(L, kind_name(ev.kind));
        lua_setfield(L, -2, "kind");

        lua_pushinteger(L, static_cast<lua_Integer>(ev.id));
        lua_setfield(L, -2, "id");

        lua_pushinteger(L, static_cast<lua_Integer>(ev.related_id));
        lua_setfield(L, -2, "related_id");

        push_endpoint(L, ev.peer);
        lua_setfield(L, -2, "peer");

        const char* payload = "";
        if (!ev.payload.empty())
        {
            payload = reinterpret_cast<const char*>(ev.payload.data());
        }
        lua_pushlstring(L, payload, ev.payload.size());
        lua_setfield(L, -2, "data");

        lua_pushinteger(L, ev.payload.size());
        lua_setfield(L, -2, "size");

        lua_pushinteger(L, ev.error_code);
        lua_setfield(L, -2, "error_code");

        lua_pushlstring(L, ev.error_text.data(), ev.error_text.size());
        lua_setfield(L, -2, "error_text");
    }

    void close_state(const reactor_handle& handle)
    {
        if (!handle || handle->closed)
        {
            return;
        }

        handle->closed = true;
        if (handle->reactor)
        {
            handle->reactor->stop();
        }

        if (handle->owner && handle->callback_ref != LUA_NOREF)
        {
            luaL_unref(handle->owner, LUA_REGISTRYINDEX, handle->callback_ref);
            handle->callback_ref = LUA_NOREF;
        }
    }

    void invoke_callback(const reactor_handle& handle, const anet::event& ev)
    {
        if (!handle || handle->closed || !handle->owner || handle->callback_ref == LUA_NOREF)
        {
            return;
        }

        lua_State* L = handle->owner;
        lua_rawgeti(L, LUA_REGISTRYINDEX, handle->callback_ref);
        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            return;
        }

        push_event(L, ev);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK)
        {
            const char* err = lua_tostring(L, -1);
            alog::error("anet Lua callback failed: {}", err ? err : "unknown");
            lua_pop(L, 1);
        }
    }

    void install_handler(const reactor_handle& handle)
    {
        std::weak_ptr<lua_reactor_state> weak = handle;
        handle->reactor->set_handler(
            [weak](const anet::event& ev)
            {
                auto locked = weak.lock();
                if (!locked)
                {
                    return;
                }
                invoke_callback(locked, ev);
            }
        );
    }

    anet::endpoint read_endpoint(lua_State* L, int host_idx, int port_idx)
    {
        anet::endpoint ep {};
        const char* host = luaL_checkstring(L, host_idx);
        ep.host = host ? host : "0.0.0.0";
        ep.port = static_cast<std::uint16_t>(luaL_checkinteger(L, port_idx));
        return ep;
    }

    anet::reactor_options read_options(lua_State* L, int idx)
    {
        anet::reactor_options options {};
        if (!lua_istable(L, idx))
        {
            return options;
        }

        auto read_integer = [&](const char* name, auto setter)
        {
            lua_getfield(L, idx, name);
            if (lua_isinteger(L, -1))
            {
                setter(static_cast<std::int64_t>(lua_tointeger(L, -1)));
            }
            lua_pop(L, 1);
        };

        read_integer("max_events", [&](std::int64_t v) {
            options.max_events = static_cast<int>(v);
        });
        read_integer("epoll_wait_ms", [&](std::int64_t v) {
            options.epoll_wait_ms = static_cast<int>(v);
        });
        read_integer("read_chunk_size", [&](std::int64_t v) {
            options.read_chunk_size = static_cast<std::size_t>(v);
        });
        read_integer("max_tcp_packet_size", [&](std::int64_t v) {
            options.max_tcp_packet_size = static_cast<std::size_t>(v);
        });
        read_integer("connect_timeout_ms", [&](std::int64_t v) {
            options.connect_timeout = std::chrono::milliseconds(v);
        });
        read_integer("idle_timeout_ms", [&](std::int64_t v) {
            options.idle_timeout = std::chrono::milliseconds(v);
        });

        return options;
    }

    int push_fail(lua_State* L, const std::string& error_text)
    {
        lua_pushnil(L);
        lua_pushlstring(L, error_text.data(), error_text.size());
        return 2;
    }

    int push_fail(lua_State* L, const reactor_handle& handle)
    {
        if (handle && handle->reactor)
        {
            return push_fail(L, handle->reactor->last_error());
        }
        return push_fail(L, "invalid anet reactor");
    }

    int lua_reactor_new(lua_State* L)
    {
        auto options = read_options(L, 1);
        auto* holder = static_cast<reactor_handle*>(lua_newuserdatauv(L, sizeof(reactor_handle), 0));
        new (holder) reactor_handle(std::make_shared<lua_reactor_state>());

        (*holder)->reactor = std::make_unique<anet::reactor>(options);
        (*holder)->owner = L;
        install_handler(*holder);

        luaL_getmetatable(L, k_reactor_mt);
        lua_setmetatable(L, -2);
        return 1;
    }

    int lua_reactor_gc(lua_State* L)
    {
        auto* holder = static_cast<reactor_handle*>(luaL_checkudata(L, 1, k_reactor_mt));
        if (holder)
        {
            close_state(*holder);
            holder->~reactor_handle();
        }
        return 0;
    }

    int lua_reactor_destroy(lua_State* L)
    {
        close_state(get_handle(L, 1));
        return 0;
    }

    int lua_reactor_on(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        if (handle->closed)
        {
            return push_fail(L, "reactor already closed");
        }

        if (lua_isnoneornil(L, 2))
        {
            if (handle->callback_ref != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, handle->callback_ref);
                handle->callback_ref = LUA_NOREF;
            }
            lua_pushboolean(L, 1);
            return 1;
        }

        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        if (handle->callback_ref != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle->callback_ref);
        }
        handle->callback_ref = ref;
        handle->owner = L;
        handle->reactor->bind_actor_thread(athd::getct(), "anet_event");

        lua_pushboolean(L, 1);
        return 1;
    }

    int lua_reactor_start(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        if (handle->closed)
        {
            return push_fail(L, "reactor already closed");
        }

        if (!handle->reactor->start())
        {
            return push_fail(L, handle);
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    int lua_reactor_stop(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        if (!handle->closed && handle->reactor)
        {
            handle->reactor->stop();
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    int lua_reactor_is_running(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        lua_pushboolean(L, handle->reactor && handle->reactor->is_running());
        return 1;
    }

    int lua_reactor_listen_tcp(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        auto ep = read_endpoint(L, 2, 3);
        int backlog = 1024;
        if (!lua_isnoneornil(L, 4))
        {
            backlog = static_cast<int>(luaL_checkinteger(L, 4));
        }

        const auto id = handle->reactor->listen_tcp(ep, backlog);
        if (!id)
        {
            return push_fail(L, handle);
        }
        lua_pushinteger(L, static_cast<lua_Integer>(id));
        return 1;
    }

    int lua_reactor_connect_tcp(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        auto ep = read_endpoint(L, 2, 3);
        const auto id = handle->reactor->connect_tcp(ep);
        if (!id)
        {
            return push_fail(L, handle);
        }
        lua_pushinteger(L, static_cast<lua_Integer>(id));
        return 1;
    }

    int lua_reactor_bind_udp(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        auto ep = read_endpoint(L, 2, 3);
        const auto id = handle->reactor->bind_udp(ep);
        if (!id)
        {
            return push_fail(L, handle);
        }
        lua_pushinteger(L, static_cast<lua_Integer>(id));
        return 1;
    }

    int lua_reactor_send(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        const auto id = static_cast<anet::socket_id>(luaL_checkinteger(L, 2));
        std::size_t size = 0;
        const char* payload = luaL_checklstring(L, 3, &size);
        auto bytes = std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(payload),
            size);

        if (!handle->reactor->send_packet(id, bytes))
        {
            return push_fail(L, handle);
        }

        lua_pushboolean(L, 1);
        return 1;
    }

    int lua_reactor_sendto(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        const auto id = static_cast<anet::socket_id>(luaL_checkinteger(L, 2));
        auto ep = read_endpoint(L, 3, 4);
        std::size_t size = 0;
        const char* payload = luaL_checklstring(L, 5, &size);
        auto bytes = std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(payload),
            size);

        if (!handle->reactor->send_to(id, ep, bytes))
        {
            return push_fail(L, handle);
        }

        lua_pushboolean(L, 1);
        return 1;
    }

    int lua_reactor_close(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        const auto id = static_cast<anet::socket_id>(luaL_checkinteger(L, 2));
        bool graceful = true;
        if (!lua_isnoneornil(L, 3))
        {
            graceful = lua_toboolean(L, 3) != 0;
        }
        handle->reactor->close(id, graceful);
        lua_pushboolean(L, 1);
        return 1;
    }

    int lua_reactor_last_error(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        const auto err = handle->reactor->last_error();
        lua_pushlstring(L, err.data(), err.size());
        return 1;
    }

    int lua_reactor_tostring(lua_State* L)
    {
        auto handle = get_handle(L, 1);
        std::string text = "anet.reactor(";
        text += handle->reactor && handle->reactor->is_running() ? "running" : "stopped";
        text += ")";
        lua_pushlstring(L, text.data(), text.size());
        return 1;
    }

    void ensure_module(lua_State* L)
    {
        if (luaL_newmetatable(L, k_reactor_mt))
        {
            static const luaL_Reg methods[] = {
                {"on", lua_reactor_on},
                {"start", lua_reactor_start},
                {"stop", lua_reactor_stop},
                {"destroy", lua_reactor_destroy},
                {"is_running", lua_reactor_is_running},
                {"listen_tcp", lua_reactor_listen_tcp},
                {"connect_tcp", lua_reactor_connect_tcp},
                {"bind_udp", lua_reactor_bind_udp},
                {"send", lua_reactor_send},
                {"sendto", lua_reactor_sendto},
                {"close", lua_reactor_close},
                {"last_error", lua_reactor_last_error},
                {NULL, NULL}
            };

            luaL_newlib(L, methods);
            lua_setfield(L, -2, "__index");

            lua_pushcfunction(L, lua_reactor_gc);
            lua_setfield(L, -2, "__gc");

            lua_pushcfunction(L, lua_reactor_tostring);
            lua_setfield(L, -2, "__tostring");
        }
        lua_pop(L, 1);

        lua_getglobal(L, "anet");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
        }

        lua_pushcfunction(L, lua_reactor_new);
        lua_setfield(L, -2, "new");

        lua_pushvalue(L, -1);
        lua_setglobal(L, "anet");
        lua_pop(L, 1);
    }

    auto _ = alua::addinitfunc(
        [](lua_State* L)
        {
            ensure_module(L);
        }
    );
}
