#pragma once
#include <memory>
#include <giomm.h>
#include <glibmm.h>
#include <sigc++/connection.h>

#include "network.hpp"
#include "wifi-ap.hpp"


using type_signal_access_point = sigc::signal<void (std::shared_ptr<AccessPoint>)>;

class WifiNetwork : public Network
{
  private:
    sigc::connection access_point_signal;
    std::map<std::string, std::shared_ptr<AccessPoint>> all_access_points;
    type_signal_access_point add_ap, remove_ap;

  protected:
    std::shared_ptr<Gio::DBus::Proxy> wifi_proxy;

  public:
    WifiNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> device_proxy,
        std::shared_ptr<Gio::DBus::Proxy> wifi_proxy);
    ~WifiNetwork();

    type_signal_access_point signal_add_access_point();
    type_signal_access_point signal_remove_access_point();
    std::map<std::string, std::shared_ptr<AccessPoint>> get_access_points();
    void add_access_point(std::string path);
    void remove_access_point(std::string path);
    std::string get_name() override;
    std::string get_color_name() override;
    std::string get_icon_name() override;
    std::string get_friendly_name() override;
    std::string get_current_access_point_path();
    std::shared_ptr<AccessPoint> get_current_access_point();
};
