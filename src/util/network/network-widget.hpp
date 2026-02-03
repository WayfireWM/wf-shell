#pragma once
#include <gtkmm.h>
#include <memory>
#include "gtkmm.h"
#include "network/vpn.hpp"
#include "sigc++/connection.h"
#include "manager.hpp"
#include "wifi.hpp"

class AccessPointWidget : public Gtk::Box
{
  private:
    Gtk::Image image;
    Gtk::Label label;
    std::shared_ptr<AccessPoint> ap;
    std::string path;
    sigc::connection signal;
  public:
    AccessPointWidget(std::string path, std::shared_ptr<AccessPoint> ap);
    ~AccessPointWidget();
};

class DeviceControlWidget : public Gtk::Box
{
  private:
    std::map<std::string, std::shared_ptr<AccessPointWidget>> access_points;
    std::shared_ptr<Network> network;
    Gtk::Label label;
    Gtk::Image image;
    Gtk::Revealer revealer;
    Gtk::Box revealer_box, topbox;
  public:
    DeviceControlWidget(std::shared_ptr<Network> network);
    void add_access_point(std::shared_ptr<AccessPoint> ap);
    void remove_access_point(std::string path);
    void selected_access_point(std::string path);
    std::string type;

};

class VPNControlWidget : public Gtk::Box
{
  private:
    std::shared_ptr<VpnConfig> config;
    Gtk::Image image;
    Gtk::Label label;
  public:
    VPNControlWidget(std::shared_ptr<VpnConfig> config);
};

class NetworkControlWidget : public Gtk::Box
{
  Gtk::Box wire_box, wifi_box, mobile_box, vpn_box, bt_box, top;
  Gtk::CheckButton global_networking, wifi_networking, mobile_networking;
  std::map <std::string, std::shared_ptr<DeviceControlWidget>> widgets;
  std::map <std::string, std::shared_ptr<VPNControlWidget>> vpn_widgets;
  sigc::connection signal_network, signal_wifi, signal_mobile;
  public:
    NetworkControlWidget();
    std::shared_ptr<NetworkManager> network_manager;
    void add_device(std::shared_ptr<Network> network);
    void remove_device(std::shared_ptr<Network> network);
    void add_vpn(std::shared_ptr<VpnConfig> config);
    void remove_vpn(std::string path);
};