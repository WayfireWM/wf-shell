#include <giomm.h>
#include <glibmm.h>

#include "daemon.hpp"
#include "notification-info.hpp"

#include <iostream>

#define FDN_PATH "/org/freedesktop/Notifications"
#define FDN_NAME "org.freedesktop.Notifications"

namespace Daemon
{
static std::map<Notification::id_type, const Notification> notifications;

const auto introspection_data =
    Gio::DBus::NodeInfo::create_for_xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                        "<node name=\"" FDN_PATH "\">"
                                        "    <interface name=\"" FDN_NAME "\">"

                                        "        <method name=\"GetCapabilities\">"
                                        "            <arg direction=\"out\" name=\"capabilities\"    type=\"as\"/>"
                                        "        </method>"

                                        "        <method name=\"Notify\">"
                                        "            <arg direction=\"in\"  name=\"app_name\"        type=\"s\"/>"
                                        "            <arg direction=\"in\"  name=\"replaces_id\"     type=\"u\"/>"
                                        "            <arg direction=\"in\"  name=\"app_icon\"        type=\"s\"/>"
                                        "            <arg direction=\"in\"  name=\"summary\"         type=\"s\"/>"
                                        "            <arg direction=\"in\"  name=\"body\"            type=\"s\"/>"
                                        "            <arg direction=\"in\"  name=\"actions\"         type=\"as\"/>"
                                        "            <arg direction=\"in\"  name=\"hints\"           type=\"a{sv}\"/>"
                                        "            <arg direction=\"in\"  name=\"expire_timeout\"  type=\"i\"/>"
                                        "            <arg direction=\"out\" name=\"id\"              type=\"u\"/>"
                                        "        </method>"

                                        "        <method name=\"CloseNotification\">"
                                        "            <arg direction=\"in\"  name=\"id\"              type=\"u\"/>"
                                        "        </method>"

                                        "        <method name=\"GetServerInformation\">"
                                        "            <arg direction=\"out\" name=\"name\"            type=\"s\"/>"
                                        "            <arg direction=\"out\" name=\"vendor\"          type=\"s\"/>"
                                        "            <arg direction=\"out\" name=\"version\"         type=\"s\"/>"
                                        "            <arg direction=\"out\" name=\"spec_version\"    type=\"s\"/>"
                                        "        </method>"

                                        "        <signal name=\"NotificationClosed\">"
                                        "            <arg name=\"id\"         type=\"u\"/>"
                                        "            <arg name=\"reason\"     type=\"u\"/>"
                                        "        </signal>"

                                        "        <signal name=\"ActionInvoked\">"
                                        "            <arg name=\"id\"         type=\"u\"/>"
                                        "            <arg name=\"action_key\" type=\"s\"/>"
                                        "        </signal>"
                                        "    </interface>"
                                        "</node>") -> lookup_interface();

bool is_running = false;
guint owner_id = 0;

// used to emit dbus signals
Glib::RefPtr<Gio::DBus::Connection> connection;

notification_signal signal_notification_new;
notification_signal signal_notification_replaced;
notification_signal signal_notification_closed;

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

sigc::signal<void> signal_daemon_stopped;

sigc::signal<void> signalDaemonStopped()
{
    return signal_daemon_stopped;
}

using DBusMethod = void (*)(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &sender,
                            const Glib::VariantContainerBase &parameters,
                            const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation);

#define dbus_method(name)                                                                                            \
    static void dbusMethod##name(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &sender, \
                                 const Glib::VariantContainerBase &parameters,                                       \
                                 const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation)

dbus_method(GetCapabilities)
{
    static const auto value = Glib::Variant<std::tuple<std::vector<Glib::ustring>>>::create(
        {{"action-icons", "actions", "body", "body-images", "persistance"}});
    invocation->return_value(value);
    connection->flush();
}

dbus_method(Notify)
{
    const auto notification = Notification(parameters, sender);
    const auto id = notification.id;
    const auto id_var = Glib::VariantContainerBase::create_tuple(Glib::Variant<Notification::id_type>::create(id));

    invocation->return_value(id_var);
    connection->flush();

    bool is_replacing = notifications.count(id) == 1;
    if (is_replacing)
    {
        notifications.erase(id);
    }
    notifications.insert({id, notification});

    if (is_replacing)
    {
        signal_notification_replaced.emit(id);
    }
    else
    {
        signal_notification_new.emit(id);
    }
}

dbus_method(CloseNotification)
{
    Glib::VariantBase id_var;
    parameters.get_child(id_var, 0);
    closeNotification(Glib::VariantBase::cast_dynamic<Glib::Variant<Notification::id_type>>(id_var).get(),
                      CloseReason::MethodCalled);
}

dbus_method(GetServerInformation)
{
    static const auto info =
        Glib::Variant<std::tuple<Glib::ustring, Glib::ustring, Glib::ustring, Glib::ustring>>::create(
            {"wf-panel", "wayfire.org", "0.8.0", "1.2"});
    invocation->return_value(info);
    connection->flush();
}

#undef dbus_method

void on_interface_method_call(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &sender,
                              const Glib::ustring &object_path, const Glib::ustring &interface_name,
                              const Glib::ustring &method_name, const Glib::VariantContainerBase &parameters,
                              const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation)
{

#define DBUS_METHOD_PAIR(name)  \
    {                           \
#name, dbusMethod##name \
    }

    static const std::map<Glib::ustring, DBusMethod> methods = {
        DBUS_METHOD_PAIR(GetCapabilities), DBUS_METHOD_PAIR(Notify), DBUS_METHOD_PAIR(CloseNotification),
        DBUS_METHOD_PAIR(GetServerInformation)};

#undef DBUS_METHOD_PAIR

    if (methods.count(method_name) != 0)
        methods.at(method_name)(connection, sender, parameters, invocation);
    else
        std::cerr << "Notifications: Error: no such method " << method_name << "\n";
}

const auto interface_vtable = Gio::DBus::InterfaceVTable(&on_interface_method_call);

void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name)
{
    connection->register_object(FDN_PATH, introspection_data, interface_vtable);
}

void on_name_acquired(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name)
{
    if (name == FDN_NAME)
        Daemon::connection = connection;
}

void on_name_lost(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name)
{
    std::cerr << "Notifications: Error: DBus connection name has been lost.\n";
    stop();
}

void start()
{
    if (owner_id == 0)
    {
        owner_id = Gio::DBus::own_name(Gio::DBus::BUS_TYPE_SESSION, FDN_NAME, &on_bus_acquired, &on_name_acquired,
                                       &on_name_lost, Gio::DBus::BUS_NAME_OWNER_FLAGS_NONE);
    }
}

void stop()
{
    signal_daemon_stopped.emit();
    Gio::DBus::unown_name(owner_id);
    owner_id = 0;
    notifications.clear();
}

const std::map<Notification::id_type, const Notification> &getNotifications()
{
    return notifications;
}

void closeNotification(Notification::id_type id, CloseReason reason)
{
    if (notifications.count(id) == 0)
        return;
    signal_notification_closed.emit(id);
    const auto &notification = notifications.at(id);
    if (connection)
    {
        auto body = Glib::Variant<std::tuple<guint32, guint32>>::create({id, reason});
        connection->emit_signal(FDN_PATH, FDN_NAME, "NotificationClosed", notification.additional_info.sender, body);
    }
}

void invokeAction(Notification::id_type id, const std::string &action_key)
{
    if (notifications.count(id) == 0)
        return;
    if (connection)
    {
        auto body = Glib::Variant<std::tuple<guint32, std::string>>::create({id, action_key});
        connection->emit_signal(FDN_PATH, FDN_NAME, "ActionInvoked", notifications.at(id).additional_info.sender, body);
    }
}
} // namespace Daemon
