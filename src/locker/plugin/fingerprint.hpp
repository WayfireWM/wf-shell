#ifndef LOCKER_FINGERPRINT_PLUGIN_HPP
#define LOCKER_FINGERPRINT_PLUGIN_HPP

#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <unordered_map>
#include <giomm.h>

#include "../plugin.hpp"
#include "giomm/dbusproxy.h"
#include "glibmm/refptr.h"

class WayfireLockerFingerprintPlugin : public WayfireLockerPlugin
{
  public:
    guint dbus_name_id;
    Glib::RefPtr<Gio::DBus::Proxy> device_proxy;

    WayfireLockerFingerprintPlugin();
    ~WayfireLockerFingerprintPlugin();
    void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name);
    void on_device_acquired(const Glib::RefPtr<Gio::AsyncResult> & result);
    void start_fingerprint_scanning();
    void add_output(int id, Gtk::Grid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;

    bool enable;
    bool is_scanning;
    void update_labels(std::string text);
    void update_image(std::string image);

    std::unordered_map<int, std::shared_ptr<Gtk::Label>> labels;
    std::unordered_map<int, std::shared_ptr<Gtk::Image>> images;
    std::string icon_contents  = "";
    std::string label_contents = "";
    std::string finger_name    = "";
};

#endif
