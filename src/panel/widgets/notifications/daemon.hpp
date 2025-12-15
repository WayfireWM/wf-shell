#ifndef NOTIFICATION_DAEMON_HPP
#define NOTIFICATION_DAEMON_HPP

#include "notification-info.hpp"

#include <giomm/dbusconnection.h>

#include <set>

#define dbus_method(name)                                                                                              \
    void name ## dbus_method(const Glib::ustring & sender, const Glib::VariantContainerBase & parameters,                  \
    const Glib::RefPtr<Gio::DBus::MethodInvocation> & invocation)
class Daemon
{
  public:
    enum CloseReason : guint32
    {
        Expired      = 1,
        Dismissed    = 2,
        MethodCalled = 3,
        Undefined    = 4,
    };

    using notification_signal = sigc::signal<void (Notification::id_type)>;

    notification_signal signalNotificationNew()
    {
        return signal_notification_new;
    }

    notification_signal signalNotificationReplaced()
    {
        return signal_notification_replaced;
    }

    notification_signal signalNotificationClosed()
    {
        return signal_notification_closed;
    }

    /*!
     * Initializes and launches the daemon.
     *
     * Returns a shared pointer to the instance.
     * Once there are no alive shared pointers to the instance,
     * the daemon is automatically destroyed.
     */
    static std::shared_ptr<Daemon> Launch();

    /*!
     * Returns a pointer to the Daemon's instance if it exists
     * or an empty `shared_ptr` otherwise.
     */
    static std::shared_ptr<Daemon> Instance();

    ~Daemon();

    const std::map<Notification::id_type, const Notification> & getNotifications() const;
    void closeNotification(Notification::id_type id, CloseReason reason);
    void invokeAction(Notification::id_type id, const Glib::ustring & action_key);

  private:
    inline static std::weak_ptr<Daemon> instance;

    std::map<Notification::id_type, const Notification> notifications;

    guint owner_id = 0;
    guint object_id;
    Glib::RefPtr<Gio::DBus::Connection> daemon_connection;

    notification_signal signal_notification_new;
    notification_signal signal_notification_replaced;
    notification_signal signal_notification_closed;

    const Gio::DBus::InterfaceVTable interface_vtable{sigc::mem_fun(this, &Daemon::on_interface_method_call)};

    Daemon();

    void on_interface_method_call(const Glib::RefPtr<Gio::DBus::Connection> & connection,
        const Glib::ustring & sender,
        const Glib::ustring & object_path, const Glib::ustring & interface_name,
        const Glib::ustring & method_name, const Glib::VariantContainerBase & parameters,
        const Glib::RefPtr<Gio::DBus::MethodInvocation> & invocation);

    dbus_method(GetCapabilities);
    dbus_method(Notify);
    dbus_method(CloseNotification);
    dbus_method(GetServerInformation);

    void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name);
};

#endif
