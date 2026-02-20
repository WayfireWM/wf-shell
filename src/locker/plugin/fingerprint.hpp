#pragma once
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <gtkmm/overlay.h>
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
    Gtk::Overlay overlay;
    Gtk::Image image_print;
    Gtk::Image image_overlay;
    Gtk::Label label;
    WayfireLockerFingerprintPluginWidget(std::string label_contents, std::string image_contents,
        std::string color_contents);
};

class WayfireLockerFingerprintPlugin : public WayfireLockerPlugin
{
  private:
    DBusConnection connection;
    Glib::RefPtr<Gio::DBus::Proxy> device_proxy, manager_proxy;
    sigc::connection signal;
    sigc::connection starting_fingerprint, finding_new_device;

  public:

    WayfireLockerFingerprintPlugin();
    ~WayfireLockerFingerprintPlugin();
    void on_connection(const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name);
    void get_device();
    void on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result);
    void start_fingerprint_scanning();
    void stop_fingerprint_scanning();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    void lockout_changed(bool lockout) override;
    void hide();
    void show();
    void color(std::string color);

    bool show_state = false;
    void update(std::string text, std::string image, std::string color);

    std::map<int, std::shared_ptr<WayfireLockerFingerprintPluginWidget>> widgets;
    std::string icon_contents  = "";
    std::string label_contents = "";
    std::string color_contents = "";
};
