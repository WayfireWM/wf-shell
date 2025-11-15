#ifndef WF_IPC_HPP
#define WF_IPC_HPP

#include "gio/gio.h"
#include "giomm/outputstream.h"
#include "giomm/socketconnection.h"
#include "glibmm/iochannel.h"
#include "glibmm/refptr.h"
#include <wayfire/nonstd/json.hpp>
#include "sigc++/connection.h"
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class IIPCSubscriber
{
  public:
    virtual void on_event(wf::json_t) = 0;
};

using response_handler = std::function<void (wf::json_t)>;

class WayfireIPC
{
    std::queue<std::optional<response_handler>> response_handlers;
    std::set<IIPCSubscriber*> subscribers;
    std::unordered_map<std::string, std::set<IIPCSubscriber*>> subscriptions;
    uint32_t length;
    sigc::connection sig_connection;
    Glib::RefPtr<Gio::SocketConnection> connection;
    Glib::RefPtr<Gio::InputStream> input;
    Glib::RefPtr<Gio::OutputStream> output;
    std::queue<std::string> write_queue;
    bool writing = false;

    void connect();
    void disconnect();
    void send_message(const std::string& message);
    bool send_queue(Glib::IOCondition cond);
    bool receive(Glib::IOCondition cond);
    void write_stream(const std::string& message);
    void write_next();

  public:
    void send(const std::string& message);
    void send(const std::string& message, response_handler);
    void subscribe(IIPCSubscriber *subscriber, const std::vector<std::string>& events);
    void subscribe_all(IIPCSubscriber *subscriber);
    void unsubscribe(IIPCSubscriber *subscriber);
    WayfireIPC();
    ~WayfireIPC();
};

class WayfireIPCManager
{
    uint counter    = 0;
    WayfireIPC *ipc = nullptr;
    void release();

  public:
    std::shared_ptr<WayfireIPC> get_IPC();
};

#endif // WF_IPC_HPP
