#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <glibmm-2.4/glibmm/iochannel.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <wayfire/util/log.hpp>

#include "wf-ipc.hpp"
#include "giomm/error.h"
#include "giomm/socketclient.h"
#include "giomm/unixsocketaddress.h"
#include "glibconfig.h"
#include "glibmm/error.h"
#include "glibmm/iochannel.h"
#include "glibmm/main.h"
#include "sigc++/functors/mem_fun.h"

WayfireIPC::WayfireIPC()
{
    connect();

    sig_connection = Glib::signal_io().connect(
        sigc::mem_fun(*this, &WayfireIPC::receive),
        connection->get_socket()->get_fd(),
        Glib::IO_IN);
}

WayfireIPC::~WayfireIPC()
{
    disconnect();
}

void WayfireIPC::connect()
{
    const char *socket_path = getenv("WAYFIRE_SOCKET");
    if (socket_path == nullptr)
    {
        throw std::runtime_error("Wayfire socket not found");
    }

    auto client  = Gio::SocketClient::create();
    auto address = Gio::UnixSocketAddress::create(socket_path);
    connection = client->connect(address);
    connection->get_socket()->set_blocking(false);
    output = connection->get_output_stream();
    input  = connection->get_input_stream();
}

void WayfireIPC::disconnect()
{
    sig_connection.disconnect();
    input->close();
    output->close();
    connection->close();
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
    if (output->has_pending())
    {
        write_queue.push(message);
        write_next();
        return;
    }

    // Shortcut: stream is not busy, no queue needed
    write_stream(message);
}

void WayfireIPC::write_next()
{
    if (writing)
    {
        return;
    }

    writing = true;
    sig_connection = Glib::signal_io().connect(
        sigc::mem_fun(*this, &WayfireIPC::send_queue),
        connection->get_socket()->get_fd(),
        Glib::IO_OUT);
}

void WayfireIPC::write_stream(const std::string& message)
{
    try {
        writing = true;
        uint32_t length = message.size();
        auto all_data   = std::make_shared<std::string>((char*)&length, 4);
        *all_data += message;
        output->write_all_async(all_data->data(), all_data->size(),
            [this, all_data] (Glib::RefPtr<Gio::AsyncResult>& result)
        {
            try {
                gsize written;
                auto success = output->write_all_finish(result, written);
                if (!success)
                {
                    LOGE("Write failed. Bytes written: ", written);
                }

                this->writing = false;
                write_next();
            } catch (const Glib::Error& e)
            {
                LOGE("Write failed: ", e.what());
            }
        });
    } catch (const Gio::Error& e)
    {
        LOGE("GIO Error: ", e.what());
    }
}

bool WayfireIPC::send_queue(Glib::IOCondition cond)
{
    if (write_queue.empty())
    {
        writing = false;
        return false;
    }

    auto message = write_queue.front();
    write_queue.pop();

    write_stream(message);
    return false;
}

bool WayfireIPC::receive(Glib::IOCondition cond)
{
    try {
        ssize_t received = 0;

        while (connection->get_socket()->get_available_bytes() > 0)
        {
            received = input->read(&length, 4);
            if (received == -1)
            {
                LOGE("Receive message length failed");
                return false;
            }

            if (received == 0)
            {
                LOGE("Disconnected");
                return false;
            }

            std::string buf(length, 0);
            received = input->read(&buf[0], length);
            if (received == -1)
            {
                LOGE("Receive message body failed");
                return false;
            }

            if (received == 0)
            {
                LOGE("Disconnected");
                return false;
            }

            wf::json_t message;
            auto err = wf::json_t::parse_string(buf, message);
            if (err.has_value())
            {
                LOGE("Parse error: ", err.value(), " message: ", buf, " length: ", buf.length());
                return false;
            }

            if (message.has_member("event"))
            {
                for (auto subscriber : subscribers)
                {
                    subscriber->on_event(message);
                }

                if (subscriptions.find(message["event"]) != subscriptions.end())
                {
                    for (auto sub : subscriptions[message["event"]])
                    {
                        sub->on_event(message);
                    }
                }
            } else
            {
                auto handler = response_handlers.front();
                response_handlers.pop();
                if (handler.has_value())
                {
                    handler.value()(message);
                }
            }
        }
    } catch (const Gio::Error& e)
    {
        LOGE("GIO Error: ", e.what());
        return false;
    }

    return true;
}

void WayfireIPC::subscribe_all(IIPCSubscriber *subscriber)
{
    subscribers.insert(subscriber);

    wf::json_t new_subs;
    new_subs["method"] = "window-rules/events/watch";
    send(new_subs.serialize());
}

void WayfireIPC::subscribe(IIPCSubscriber *subscriber, const std::vector<std::string>& events)
{
    wf::json_t new_subs;
    new_subs["method"] = "window-rules/events/watch";
    new_subs["events"] = wf::json_t::array();

    for (auto event : events)
    {
        if (subscriptions.find(event) == subscriptions.end())
        {
            new_subs["events"].append(event);
            subscriptions[event] = std::set<IIPCSubscriber*>();
        }

        subscriptions[event].insert(subscriber);
    }

    if (new_subs["events"].size() > 0)
    {
        send(new_subs.serialize());
    }
}

void WayfireIPC::unsubscribe(IIPCSubscriber *subscriber)
{
    subscribers.erase(subscriber);

    for (auto& [_, subs] : subscriptions)
    {
        subs.erase(subscriber);
    }
}

// WayfireIPCManager
std::shared_ptr<WayfireIPC> WayfireIPCManager::get_IPC()
{
    if (ipc == nullptr)
    {
        ipc = new WayfireIPC();
    }

    counter++;
    return std::shared_ptr<WayfireIPC>(ipc, [this] (WayfireIPC*)
    {
        release();
    });
}

void WayfireIPCManager::release()
{
    counter--;
    if (counter == 0)
    {
        delete ipc;
        ipc = nullptr;
    }
}
