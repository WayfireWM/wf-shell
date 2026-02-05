#pragma once
#include "sigc++/connection.h"
#include <memory>
#include <string>
#include <giomm.h>
#include <glibmm.h>

#define NM_DEVICE_STATE_UNKNOWN 0
#define NM_DEVICE_STATE_UNMANAGED 10
#define NM_DEVICE_STATE_UNAVAILABLE 20
#define NM_DEVICE_STATE_DISCONNECTED 30
#define NM_DEVICE_STATE_PREPARE 40
#define NM_DEVICE_STATE_CONFIG 50
#define NM_DEVICE_STATE_NEED_AUTH 60
#define NM_DEVICE_STATE_IP_CONFIG 70
#define NM_DEVICE_STATE_IP_CHECK 80
#define NM_DEVICE_STATE_SECONDARIES 90
#define NM_DEVICE_STATE_ACTIVATED 100
#define NM_DEVICE_STATE_DEACTIVATING 110
#define NM_DEVICE_STATE_FAILED 120

using type_signal_network_altered = sigc::signal<void (void)>;

class Network
{
  protected:
    type_signal_network_altered network_altered;
    std::string network_path;
    std::vector<sigc::connection> signals;

  public:
    type_signal_network_altered signal_network_altered();
    std::shared_ptr<Gio::DBus::Proxy> device_proxy;
    std::string interface = "";
    int last_state = 0;
    virtual std::string get_name() = 0;
    virtual std::vector<std::string> get_css_classes() = 0;
    Network(std::string path, std::shared_ptr<Gio::DBus::Proxy> in_proxy);
    ~Network();
    virtual std::string get_friendly_name();
    virtual std::string get_interface();
    bool show_spinner();
    virtual std::string get_icon_name() = 0;
    std::string get_icon_symbolic()
    {
        return get_icon_name() + "-symbolic";
    }

    virtual std::string get_secure_variant()
    {
        return "";
    }

    std::string get_path();
    void disconnect();
    void connect(std::string path_extra);
    void toggle();
    bool is_active();
    Network(const Network &) = delete;
    Network& operator =(const Network&) = delete;
};
