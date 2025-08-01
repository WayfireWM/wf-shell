#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <vector>

#include "wf-ipc.hpp"
#include "glibmm/iochannel.h"
#include "glibmm/main.h"
#include "sigc++/functors/mem_fun.h"

WayfireIPC::WayfireIPC(): socket_fd(-1)
{
    connect();

    auto connection = Glib::signal_io().connect(
        sigc::mem_fun(this, &WayfireIPC::receive), socket_fd, Glib::IO_IN
    );
}

WayfireIPC::~WayfireIPC()
{
    disconnect();
}

void WayfireIPC::connect()
{
    const char* socket_path = getenv("WAYFIRE_SOCKET");
    if (socket_path == nullptr)
    {
        throw std::runtime_error("Wayfire socket not found");
    }

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        throw std::runtime_error("Wayfire socket create error");
    }

    if (fcntl(socket_fd, F_SETFL, O_NONBLOCK) == -1)
    {
        throw std::runtime_error("Wayfire socket set flag error");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (strlen(socket_path) >= sizeof(addr.sun_path))
    {
        throw std::runtime_error("Wayfire socket path too long");
    }

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (::connect(socket_fd, (sockaddr*)&addr, sizeof(addr)) == -1)
    {
        throw std::runtime_error("Wayfire socket connect error");
    }
}

void WayfireIPC::disconnect()
{
    if (socket_fd != -1)
    {
        close(socket_fd);
        socket_fd = -1;
    }
}

void WayfireIPC::send(const std::string& message)
{
    send_message(message);
    response_handlers.push(std::nullopt);
}

void WayfireIPC::send(const std::string& message, response_handler cb)
{
    send_message(message);
    response_handlers.push(cb);
}

void WayfireIPC::send_message(const std::string& message)
{
    size_t sent = 0;
    while (sent < message.size()) {
        uint32_t length = message.size();
        auto all_data = std::string((char*)&length, 4);
        all_data += message;
        ssize_t res = ::send(socket_fd, all_data.data() + sent, 
                            all_data.size() - sent, MSG_NOSIGNAL);
        if (res == -1) {
            throw std::system_error(errno, std::system_category(), "send failed");
        }

        sent += res;
    }
}

bool WayfireIPC::receive(Glib::IOCondition cond)
{
    ssize_t received = 0;

    if (!length_received)
    {
        received = ::recv(socket_fd, &length, 4, 0);
        if (received == -1) {
            return true;
        }
        if (received == 0) {
            throw std::runtime_error("Connection closed by peer");
        }
    }
    length_received = true;

    std::string buf(length + 1, 0);
    received = ::recv(socket_fd, &buf[0], length, 0);
    if (received == -1) {
        return true;
    }
    if (received == 0) {
        throw std::runtime_error("Connection closed by peer");
    }
    length_received = false;

    auto message = nlohmann::json::parse(buf);

    if (message.contains("event")) {
        for (auto subscriber : subscribers) {
            subscriber->on_event(message);
        }

        if (subscriptions.find(message["event"]) != subscriptions.end()) {
            for (auto sub : subscriptions[message["event"]]) {
                sub->on_event(message);
            }
        }
    } else {
        auto handler = response_handlers.front();
        response_handlers.pop();
        if (handler.has_value()) {
            handler.value()(message);
        }
    }
    
    return true;
}

void WayfireIPC::subscribe_all(IIPCSubscriber* subscriber)
{
    subscribers.insert(subscriber);

    nlohmann::json new_subs;
    new_subs["method"] = "window-rules/events/watch";
    send(new_subs.dump());
}

void WayfireIPC::subscribe(IIPCSubscriber* subscriber, const std::vector<std::string>& events)
{
    nlohmann::json new_subs;
    new_subs["method"] = "window-rules/events/watch";
    new_subs["events"] = nlohmann::json::array();

    for (auto event : events) {
        if (subscriptions.find(event) == subscriptions.end()) {
            new_subs["events"].push_back(event);
            subscriptions[event] = std::set<IIPCSubscriber*>();
        }

        subscriptions[event].insert(subscriber);
    }

    if (new_subs["events"].size() > 0) {
        send(new_subs.dump());
    }
}

void WayfireIPC::unsubscribe(IIPCSubscriber* subscriber)
{
    subscribers.erase(subscriber);

    for (auto& [_, subs] : subscriptions) {
        subs.erase(subscriber);
    }
}

