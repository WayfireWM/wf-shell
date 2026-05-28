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
    Glib::RefPtr<Gio::DBus::Proxy> device_proxy, manager_proxy, dbus_proxy;
    sigc::connection signal, fprint_watcher;
    sigc::connection starting_fingerprint, finding_new_device;

    bool getting_fprintd = false;
    void get_fprintd();

  public:

    WayfireLockerFingerprintPlugin();
    ~WayfireLockerFingerprintPlugin();
    void on_connection(const Glib::RefPtr<Gio::AsyncResult> & result);
    void get_device();
    void on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result);
    void on_list_fingerprints(Glib::RefPtr<Gio::AsyncResult>& result);
    void start_fingerprint_scanning();
    void stop_fingerprint_scanning();
    void add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    void lockout_changed(bool lockout) override;
    void failure() override;
    void hide();
    void show();
    void color(std::string color);

    bool show_state = false;
    void update(std::string text, std::string image, std::string color);

    std::map<std::string, std::shared_ptr<WayfireLockerFingerprintPluginWidget>> widgets;
    std::string icon_contents  = "";
    std::string label_contents = "";
    std::string color_contents = "";
};
