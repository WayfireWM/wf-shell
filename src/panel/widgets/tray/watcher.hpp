#ifndef TRAY_WATCHER_HPP
#define TRAY_WATCHER_HPP

#include <memory>

#include <giomm.h>
#include <libdbusmenu-gtk/dbusmenu-gtk.h>

/*
 * Singleton representing a StatusNotifierWatceher instance.
 */
class Watcher
{
    public:
    /*!
     * Initializes and launches the watcher.
     */
    static void Launch();

    ~Watcher();

    private:
    static constexpr auto SNW_PATH = "/org/freedesktop/StatusNotifierWatcher";
    static constexpr auto SNW_NAME = "org.freedesktop.StatusNotifierWatcher";

    inline static std::unique_ptr<Watcher> instance = nullptr;

    guint dbus_name_id;
    Glib::RefPtr<Gio::DBus::Connection> watcher_connection;

    std::set<Glib::RefPtr<Gio::DBus::Proxy>> sn_items;
    std::set<Glib::RefPtr<Gio::DBus::Proxy>> sn_hosts;

    const Gio::DBus::InterfaceVTable interface_table = Gio::DBus::InterfaceVTable(
        [this](auto &&...args) { on_interface_method_call(std::forward<decltype(args)>(args)...); },
        [this](auto &&...args) { on_interface_get_property(std::forward<decltype(args)>(args)...); });

    Watcher();

    void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name);

    void on_interface_method_call(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &sender,
                                  const Glib::ustring &object_path, const Glib::ustring &interface_name,
                                  const Glib::ustring &method_name, const Glib::VariantContainerBase &parameters,
                                  const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation);

    void on_interface_get_property(Glib::VariantBase &property, const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                   const Glib::ustring &sender, const Glib::ustring &object_path,
                                   const Glib::ustring &interface_name, const Glib::ustring &property_name);

    void register_status_notifier_item(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                       const Glib::ustring &service);
    void register_status_notifier_host(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                       const Glib::ustring &service);

    template <typename... Args>
    void emit_signal(const Glib::ustring &name, Args &&...args)
    {
        watcher_connection->emit_signal(
            SNW_PATH, SNW_PATH, name, {},
            Glib::Variant<std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>>::create(
                std::tuple(std::forward<Args>(args)...)));
    }
}; // namespace Watcher

#endif
