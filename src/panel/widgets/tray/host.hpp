#ifndef TRAY_HOST_HPP
#define TRAY_HOST_HPP

#include "watcher.hpp"

#include <giomm.h>

class WayfireStatusNotifier;

class StatusNotifierHost
{
    inline static int hosts_counter = 0;

    std::shared_ptr<Watcher> watcher_ptr = Watcher::Launch();

    guint dbus_name_id;

    guint watcher_id;
    Glib::RefPtr<Gio::DBus::Proxy> watcher_proxy;

    WayfireStatusNotifier *tray;

    void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name);
    void register_host(const Glib::RefPtr<Gio::AsyncResult> & result);

  public:
    explicit StatusNotifierHost(WayfireStatusNotifier *tray);
    ~StatusNotifierHost();
};

#endif
