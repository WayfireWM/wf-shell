#pragma once
#include <memory>
#include <sigc++/connection.h>
#include <gtkmm.h>

#include "gtk/gtk.h"
#include "gtkmm/overlay.h"
#include "manager.hpp"
#include "wifi-ap.hpp"
#include "vpn.hpp"
#include <glibmm/extraclassinit.h>
#include <vector>


class MenuItemBox : public Gtk::Widget
{
  public:
    MenuItemBox();
    ~MenuItemBox();
    sigc::signal<void()> signal_activate();

    void append(Gtk::Widget& child);
    void set_orientation(Gtk::Orientation orientation);

  protected:
    std::vector<sigc::connection> menu_signals;
    sigc::signal<void()> m_signal_activate;

    virtual bool on_key_pressed(guint keyval, guint keycode, Gdk::ModifierType state);
    virtual bool on_focus(GtkDirectionType direction);
};

class AccessPointWidget : public MenuItemBox
{
  private:
    Gtk::Overlay overlay;
    Gtk::Image wifi, security;
    Gtk::Label label, band;
    std::shared_ptr<AccessPoint> ap;
    std::string path;

  public:
    std::vector<sigc::connection> signals;
    AccessPointWidget(std::string path, std::shared_ptr<AccessPoint> ap);
    ~AccessPointWidget();
    std::shared_ptr<AccessPoint> get_ap();
};

class DeviceControlWidget : public MenuItemBox
{
  private:
    std::map<std::string, std::shared_ptr<AccessPointWidget>> access_points;
    std::shared_ptr<Network> network;
    Gtk::Label label;
    Gtk::Image image;
    Gtk::Revealer revealer;
    Gtk::Box revealer_box, topbox;
    std::vector<sigc::connection> signals;

  public:
    DeviceControlWidget(std::shared_ptr<Network> network);
    ~DeviceControlWidget();
    void add_access_point(std::shared_ptr<AccessPoint> ap);
    void remove_access_point(std::string path);
    void selected_access_point(std::string path, std::shared_ptr<AccessPointWidget> widget);
    void sort_access_points();
    std::string type;
};

class VPNControlWidget : public MenuItemBox
{
  private:
    std::shared_ptr<VpnConfig> config;
    Gtk::Image image;
    Gtk::Label label;
    std::shared_ptr<NetworkManager> network_manager;

  public:
    std::vector<sigc::connection> signals;

    VPNControlWidget(std::shared_ptr<VpnConfig> config);
    ~VPNControlWidget();
};

class NetworkControlWidget : public Gtk::Box
{
    Gtk::Label network_manager_failed;
    Gtk::Box wire_box, wifi_box, mobile_box, vpn_box, bt_box, top;
    Gtk::CheckButton global_networking, wifi_networking, mobile_networking;
    std::map<std::string, std::shared_ptr<DeviceControlWidget>> widgets;
    std::map<std::string, std::shared_ptr<VPNControlWidget>> vpn_widgets;
    sigc::connection signal_network, signal_wifi, signal_mobile;
    std::vector<sigc::connection> signals;

  public:
    NetworkControlWidget();
    ~NetworkControlWidget();
    std::shared_ptr<NetworkManager> network_manager;
    void update_globals();
    void add_device(std::shared_ptr<Network> network);
    void remove_device(std::shared_ptr<Network> network);
    void add_vpn(std::shared_ptr<VpnConfig> config);
    void remove_vpn(std::string path);
    void nm_start();
    void nm_stop();
    void mm_start();
    void mm_stop();
};
