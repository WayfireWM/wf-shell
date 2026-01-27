#pragma once
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <giomm.h>
#include <giomm/dbusproxy.h>
#include <glibmm/refptr.h>
#include <sigc++/connection.h>

#include "lockergrid.hpp"
#include "plugin.hpp"
#include "timedrevealer.hpp"

using DBusConnection = Glib::RefPtr<Gio::DBus::Connection>;
using DBusProxy = Glib::RefPtr<Gio::DBus::Proxy>;

class WayfireLockerFingerprintPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    Gtk::Box box;
    Gtk::Image image;
    Gtk::Label label;
    WayfireLockerFingerprintPluginWidget(std::string label_contents, std::string image_contents);
};

class WayfireLockerFingerprintPlugin : public WayfireLockerPlugin
{
  private:
    DBusConnection connection;
    Glib::RefPtr<Gio::DBus::Proxy> device_proxy, manager_proxy;
  public:

    WayfireLockerFingerprintPlugin();
    ~WayfireLockerFingerprintPlugin();
    void on_connection(const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name);
    void get_device();
    void on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result);
    void claim_device();
    void release_device();
    void start_fingerprint_scanning();
    void stop_fingerprint_scanning();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    void hide();
    void show();

    sigc::connection signal;
    sigc::connection starting_fingerprint;
    bool show_state = false;
    void update_labels(std::string text);
    void update_image(std::string image);

    std::map<int, std::shared_ptr<WayfireLockerFingerprintPluginWidget>> widgets;
    std::string icon_contents  = "";
    std::string label_contents = "";
};
