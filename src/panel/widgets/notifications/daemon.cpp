#include <giomm.h>
#include <glibmm.h>

#include "daemon.hpp"
#include "notification-center.hpp"
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
guint owner_id;
WayfireNotificationCenter *center;

using DBusMethod = void (*)(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &sender,
                            const Glib::VariantContainerBase &parameters,
                            const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation);

#define dbus_method(name)                                                                                            \
    static void dbusMethod##name(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &sender, \
                                 const Glib::VariantContainerBase &parameters,                                       \
                                 const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation)

dbus_method(GetCapabilities)
{
    static const auto value = Glib::Variant<std::tuple<std::vector<Glib::ustring>>>::create({{"body", "persistance"}});
    invocation->return_value(value);
    connection->flush();
}

dbus_method(Notify)
{
    const auto notification = Notification(parameters);
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
        center->replaceNotification(id);
    }
    else
    {
        center->newNotification(id);
    }
}

dbus_method(CloseNotification)
{
    auto id = Glib::VariantBase::cast_dynamic<Glib::Variant<Notification::id_type>>(parameters).get();
    removeNotification(id);
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

void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> &connection, Glib::ustring name)
{
    connection->register_object(FDN_PATH, introspection_data, interface_vtable);
}

void on_name_acquired(const Glib::RefPtr<Gio::DBus::Connection> &connection, Glib::ustring name)
{
}

void on_name_lost(const Glib::RefPtr<Gio::DBus::Connection> &connection, Glib::ustring name)
{
    std::cerr << "Notifications: Error: DBus connection name has been lost.\n";
    stop();
}

void start(WayfireNotificationCenter *center)
{
    if (owner_id == 0)
    {
        owner_id = Gio::DBus::own_name(Gio::DBus::BUS_TYPE_SESSION, FDN_NAME, &on_bus_acquired, &on_name_acquired,
                                       &on_name_lost, Gio::DBus::BUS_NAME_OWNER_FLAGS_NONE);
    }
    else
    {
        std::cerr << "Notifications daemon is alredy running.\n";
    }
    Daemon::center = center;
}

void stop()
{
    Gio::DBus::unown_name(owner_id);
    owner_id = 0;
    notifications.clear();
    center->onDaemonStop();
    center = nullptr;
}

void connect(WayfireNotificationCenter *center)
{
    if (owner_id == 0)
    {
        start(center);
    }
    else
    {
        // TODO(NamorNiradnug) multiple centers connected
        Daemon::center = center;
    }
}

const std::map<Notification::id_type, const Notification> &getNotifications()
{
    return notifications;
}

void removeNotification(Notification::id_type id)
{
    center->removeNotification(id);
    notifications.erase(id);
}
} // namespace Daemon
