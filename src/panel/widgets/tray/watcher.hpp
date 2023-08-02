#ifndef TRAY_WATCHER_HPP
#define TRAY_WATCHER_HPP

#include <memory>

#include <giomm.h>

/*
 * Singleton representing a StatusNotifierWatceher instance.
 */
class Watcher
{
  public:
    static constexpr auto SNW_PATH  = "/StatusNotifierWatcher";
    static constexpr auto SNW_NAME  = "org.kde.StatusNotifierWatcher";
    static constexpr auto SNW_IFACE = "org.kde.StatusNotifierWatcher";

    /*!
     * Initializes and launches the watcher, if needed.
     *
     * Returns a shared pointer to the instance.
     * Once there are no alive shared pointers to the instance,
     * the Watcher is automatically destroyed.
     */
    static std::shared_ptr<Watcher> Launch();

    /*!
     * Returns a pointer to the Watcher's instance if it exists
     * or an empty `shared_ptr` otherwise.
     */
    static std::shared_ptr<Watcher> Instance();

    ~Watcher();

  private:
    inline static std::weak_ptr<Watcher> instance;

    guint dbus_name_id;
    guint dbus_object_id;
    Glib::RefPtr<Gio::DBus::Connection> watcher_connection;

    std::map<Glib::ustring, guint> sn_items_id;
    std::map<Glib::ustring, guint> sn_hosts_id;

    const Gio::DBus::InterfaceVTable interface_table =
        Gio::DBus::InterfaceVTable(sigc::mem_fun(*this, &Watcher::on_interface_method_call),
            sigc::mem_fun(*this, &Watcher::on_interface_get_property));

    Watcher();

    void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name);

    void on_interface_method_call(const Glib::RefPtr<Gio::DBus::Connection> & connection,
        const Glib::ustring & sender,
        const Glib::ustring & object_path, const Glib::ustring & interface_name,
        const Glib::ustring & method_name, const Glib::VariantContainerBase & parameters,
        const Glib::RefPtr<Gio::DBus::MethodInvocation> & invocation);

    void on_interface_get_property(Glib::VariantBase & property,
        const Glib::RefPtr<Gio::DBus::Connection> & connection,
        const Glib::ustring & sender, const Glib::ustring & object_path,
        const Glib::ustring & interface_name, const Glib::ustring & property_name);

    void register_status_notifier_item(const Glib::RefPtr<Gio::DBus::Connection> & connection,
        const Glib::ustring & sender, const Glib::ustring & path);
    void register_status_notifier_host(const Glib::RefPtr<Gio::DBus::Connection> & connection,
        const Glib::ustring & service);

    Glib::Variant<std::vector<Glib::ustring>> get_registred_items() const;

    template<typename... Args>
    void emit_signal(const Glib::ustring & name, Args &&... args)
    {
        watcher_connection->emit_signal(
            SNW_PATH, SNW_IFACE, name, {},
            Glib::Variant<std::tuple<std::remove_cv_t<std::remove_reference_t<Args>> ...>>::create(
                std::tuple(std::forward<Args>(args)...)));
    }

    void emit_signal(const Glib::ustring& name)
    {
        watcher_connection->emit_signal(SNW_PATH, SNW_IFACE, name);
    }
}; // namespace Watcher

#endif
