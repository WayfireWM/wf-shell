#ifndef WF_IPC_HPP
#define WF_IPC_HPP

#include "glibmm/iochannel.h"
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

class IIPCSubscriber {
    public:
        virtual void on_event(nlohmann::json) = 0;
};

using response_handler = std::function<void(nlohmann::json)>;

class WayfireIPC
{
        int socket_fd;
        std::queue<std::optional<response_handler>> response_handlers;
        std::set<IIPCSubscriber*> subscribers;
        std::unordered_map<std::string, std::set<IIPCSubscriber*>> subscriptions;

        void connect();
        void disconnect();
        void send_message(const std::string& message);
        bool receive(Glib::IOCondition cond);
    public:
        void send(const std::string& message);
        void send(const std::string& message, response_handler);
        void subscribe(IIPCSubscriber* subscriber, const std::vector<std::string>& events);
        void subscribe_all(IIPCSubscriber* subscriber);
        void unsubscribe(IIPCSubscriber* subscriber);
        WayfireIPC();
        ~WayfireIPC();
};

#endif // WF_IPC_HPP