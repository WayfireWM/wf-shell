#include <giomm.h>
#include <glibmm.h>

#include "daemon.hpp"
#include "notification-info.hpp"

#include <iostream>

#define FDN_PATH "/org/freedesktop/Notifications"
#define FDN_NAME "org.freedesktop.Notifications"

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
                                                                                                   "</node>")
    ->lookup_interface();

dbus_method(Daemon::GetCapabilities)
{
    static const auto value = Glib::Variant<std::tuple<std::vector<Glib::ustring>>>::create(
        {{"action-icons", "actions", "body", "body-hyperlinks", "body-markup", "body-images",
            "persistance"}});
    invocation->return_value(value);
}

dbus_method(Daemon::Notify)
try {
    const auto notification = Notification(parameters, sender);
    const auto id     = notification.id;
    const auto id_var =
        Glib::VariantContainerBase::create_tuple(Glib::Variant<Notification::id_type>::create(id));

    invocation->return_value(id_var);

    bool is_replacing = notifications.count(id) == 1;
    if (is_replacing)
    {
        notifications.erase(id);
    }

    notifications.insert({id, notification});

    if (is_replacing)
    {
        signal_notification_replaced.emit(id);
    } else
    {
        signal_notification_new.emit(id);
    }
} catch (const std::exception & err)
{
    std::cerr << "Error at " << __PRETTY_FUNCTION__ << ": " << err.what() << '\n';
}

dbus_method(Daemon::CloseNotification)
{
    Glib::VariantBase id_var;
    parameters.get_child(id_var, 0);
    invocation->return_value(Glib::VariantContainerBase());
    Daemon::Instance()->closeNotification(
        Glib::VariantBase::cast_dynamic<Glib::Variant<Notification::id_type>>(id_var).get(),
        Daemon::CloseReason::MethodCalled);
}

dbus_method(Daemon::GetServerInformation)
{
    static const auto info =
        Glib::Variant<std::tuple<Glib::ustring, Glib::ustring, Glib::ustring, Glib::ustring>>::create(
            {"wf-panel", "wayfire.org", "0.8.0", "1.2"});
    invocation->return_value(info);
}

void Daemon::on_interface_method_call(const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & sender, const Glib::ustring & object_path,
    const Glib::ustring & interface_name, const Glib::ustring & method_name,
    const Glib::VariantContainerBase & parameters,
    const Glib::RefPtr<Gio::DBus::MethodInvocation> & invocation)
{
#define try_invoke_method(_name)                                                                                       \
    if (method_name == #_name)                                                                                         \
    _name ## dbus_method(sender, parameters, invocation)

    try_invoke_method(GetCapabilities);
    try_invoke_method(Notify);
    try_invoke_method(CloseNotification);
    try_invoke_method(GetServerInformation);
}

void Daemon::on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & name)
{
    object_id = connection->register_object(FDN_PATH, introspection_data, interface_vtable);
    daemon_connection = connection;
}

std::shared_ptr<Daemon> Daemon::Launch()
{
    if (instance.expired())
    {
        auto new_instance = std::shared_ptr<Daemon>(new Daemon());
        instance = new_instance;
        return new_instance;
    }

    return Instance();
}

std::shared_ptr<Daemon> Daemon::Instance()
{
    return instance.lock();
}

Daemon::Daemon() :
    owner_id(Gio::DBus::own_name(Gio::DBus::BUS_TYPE_SESSION, FDN_NAME,
        sigc::mem_fun(this, &Daemon::on_bus_acquired),
        {}, {}, Gio::DBus::BUS_NAME_OWNER_FLAGS_REPLACE))
{}

Daemon::~Daemon()
{
    daemon_connection->unregister_object(object_id);
    Gio::DBus::unown_name(owner_id);
}

const std::map<Notification::id_type, const Notification>& Daemon::getNotifications() const
{
    return notifications;
}

void Daemon::closeNotification(Notification::id_type id, CloseReason reason)
{
    if (notifications.count(id) == 0)
    {
        return;
    }

    signal_notification_closed.emit(id);
    const auto & notification = notifications.at(id);
    const auto body = Glib::Variant<std::tuple<guint32, guint32>>::create({id, reason});
    daemon_connection->emit_signal(FDN_PATH, FDN_NAME, "NotificationClosed",
        notification.additional_info.sender, body);
}

void Daemon::invokeAction(Notification::id_type id, const Glib::ustring & action_key)
{
    if (notifications.count(id) == 0)
    {
        return;
    }

    const auto body = Glib::Variant<std::tuple<guint32, Glib::ustring>>::create({id, action_key});
    daemon_connection->emit_signal(FDN_PATH, FDN_NAME, "ActionInvoked", notifications.at(
        id).additional_info.sender,
        body);
}
