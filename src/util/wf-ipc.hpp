#pragma once

#include <glibmm.h>
#include <giomm.h>

#include <sigc++/connection.h>
#include <functional>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <wayfire/nonstd/json.hpp>

class IIPCSubscriber
{
  public:
    virtual void on_event(wf::json_t) = 0;
};

using response_handler = std::function<void (wf::json_t)>;

class WayfireIPC;
class IPCClient
{
  private:
    int id;
    std::shared_ptr<WayfireIPC> ipc;
    std::queue<response_handler> response_handlers;

  public:
    IPCClient(int id, std::shared_ptr<WayfireIPC> ipc) : id(id), ipc(ipc)
    {}
    ~IPCClient();
    void handle_response(wf::json_t response);
    void send(const std::string& message);
    void send(const std::string& message, response_handler cb);
    void subscribe(IIPCSubscriber *subscriber, const std::vector<std::string>& events);
    void subscribe_all(IIPCSubscriber *subscriber);
    void unsubscribe(IIPCSubscriber *subscriber);
};

class WayfireIPC : public std::enable_shared_from_this<WayfireIPC>
{
  private:
    std::queue<int> response_handlers;
    std::set<IIPCSubscriber*> subscribers;
    std::unordered_map<std::string, std::set<IIPCSubscriber*>> subscriptions;
    int next_client_id{1};
    std::unordered_map<int, IPCClient*> clients;
    sigc::connection sig_connection;
    Glib::RefPtr<Gio::SocketConnection> connection;
    Glib::RefPtr<Gio::InputStream> input;
    Glib::RefPtr<Gio::OutputStream> output;
    Glib::RefPtr<Gio::Cancellable> cancel;
    std::queue<std::string> write_queue;
    bool writing = false;

    bool connect();
    void disconnect();
    void send_message(const std::string& message);
    bool send_queue(Glib::IOCondition cond);
    bool receive(Glib::IOCondition cond);
    void write_stream(const std::string& message);
    void write_next();

  public:
    void send(const std::string& message);
    void send(const std::string& message, int response_handler);
    void subscribe(IIPCSubscriber *subscriber, const std::vector<std::string>& events);
    void subscribe_all(IIPCSubscriber *subscriber);
    void unsubscribe(IIPCSubscriber *subscriber);
    std::shared_ptr<IPCClient> create_client();
    void client_destroyed(int id);

    static std::shared_ptr<WayfireIPC> get_instance();
    bool connected = false;
    WayfireIPC();
    ~WayfireIPC();
};
