#pragma once
#include <unordered_map>
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <giomm.h>
#include <giomm/dbusproxy.h>
#include <glibmm/refptr.h>

#include "lockergrid.hpp"
#include "../plugin.hpp"

class WayfireLockerFingerprintPlugin : public WayfireLockerPlugin
{
  public:
    guint dbus_name_id;
    Glib::RefPtr<Gio::DBus::Proxy> device_proxy;

    WayfireLockerFingerprintPlugin();
    ~WayfireLockerFingerprintPlugin();
    void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name);
    void on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result);
    void claim_device();
    void start_fingerprint_scanning();
    void add_output(int id, WayfireLockerGrid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;
    void hide();
    void show();

    bool enable;
    bool is_scanning;
    bool show_state = false;
    void update_labels(std::string text);
    void update_image(std::string image);

    std::unordered_map<int, std::shared_ptr<Gtk::Label>> labels;
    std::unordered_map<int, std::shared_ptr<Gtk::Image>> images;
    std::string icon_contents  = "";
    std::string label_contents = "";
};
