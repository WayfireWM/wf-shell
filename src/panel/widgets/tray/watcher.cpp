#include "watcher.hpp"

#include <giomm.h>

#include <iostream>

static const auto introspection_data = Gio::DBus::NodeInfo::create_for_xml(R"(
<?xml version="1.0" encoding="UTF-8"?>"
<node name="/org/freedesktop/StatusNotifierWatcher">
    <interface name="org.freedesktop.StatusNotifierWatcher">
        <method name="RegisterStatusNotifierItem">
            <arg direction="in" name="service" type="s"/>
        </method>
        <method name="RegisterStatusNotifierHost">
            <arg direction="in" name="service" type="s"/>
        </method>

        <property name="RegisteredStatusNotifierItems" type="as" access="read"/>
        <property name="IsStatusNotifierHostRegistered" type="b" access="read"/>
        <property name="ProtocolVersion" type="i" access="read"/>

        <signal name="StatusNotifierItemRegistered">
            <arg name="service" type="s"/>
        </signal>
        <signal name="StatusNotifierItemUnregistered">
            <arg name="service" type="s"/>
        </signal>
        <signal name="StatusNotifierHostRegistered"/>
    </interface>
</node>
)") -> lookup_interface();

Watcher::Watcher()
    : dbus_name_id(Gio::DBus::own_name(Gio::DBus::BusType::BUS_TYPE_SESSION, SNW_NAME,
                                       [this](auto... args) { on_bus_acquired(args...); }))
{
}

void Watcher::Launch()
{
    if (!instance)
    {
        instance = std::unique_ptr<Watcher>(new Watcher());
    }
}

Watcher::~Watcher()
{
    Gio::DBus::unown_name(dbus_name_id);
}

void Watcher::on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name)
{
    connection->register_object(SNW_PATH, introspection_data, interface_table);
    watcher_connection = connection;
}

void Watcher::register_status_notifier_item(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                            const Glib::ustring &service)
{
    Gio::DBus::Proxy::create(connection, service, "/StatusNotifierItem", "org.freedesktop.StatusNotifierItem",
                             [&service, this](Glib::RefPtr<Gio::AsyncResult> &result) {
                                 const auto proxy = Gio::DBus::Proxy::create_finish(result);
                                 if (!proxy)
                                 {
                                     return;
                                 }
                                 emit_signal("StatusNotifierItemRegistered", service);
                                 sn_items.insert(proxy);
                                 proxy->get_connection()->signal_closed().connect(
                                     [&proxy, this](bool remote_peer_vanished, const Glib::Error &error) {
                                         emit_signal("StatusNotifierItemUnregistered", proxy->get_name());
                                         sn_items.erase(proxy);
                                     });
                             });
}

void Watcher::register_status_notifier_host(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                            const Glib::ustring &service)
{
    Gio::DBus::Proxy::create(
        connection, service, "/StatusNotifierHost", "org.freedesktop.StatusNotifierHost",
        [&service, this](Glib::RefPtr<Gio::AsyncResult> &result) {
            const auto proxy = Gio::DBus::Proxy::create_finish(result);
            if (!proxy)
            {
                return;
            }
            emit_signal("StatusNotifierHostRegistered");
            sn_hosts.insert(proxy);
            proxy->get_connection()->signal_closed().connect(
                [&proxy, this](bool remote_peer_vanished, const Glib::Error &error) { sn_hosts.erase(proxy); });
        });
}

void Watcher::on_interface_method_call(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                       const Glib::ustring &sender, const Glib::ustring &object_path,
                                       const Glib::ustring &interface_name, const Glib::ustring &method_name,
                                       const Glib::VariantContainerBase &parameters,
                                       const Glib::RefPtr<Gio::DBus::MethodInvocation> &invocation)
{
    if (!parameters.is_of_type(Glib::VariantType("(s)")))
    {
        std::cerr << "StatusNotifierWatcher: invalid argument type: expected 's', got " << parameters.get_type_string()
                  << std::endl;
    }
    Glib::Variant<Glib::ustring> service_variant;
    parameters.get_child(service_variant, 0);
    const auto service = service_variant.get();
    if (method_name == "RegisterStatusNotifierItem")
    {
        register_status_notifier_item(connection, service);
    }
    else if (method_name == "RegisterStatusNotifierHost")
    {
        register_status_notifier_host(connection, service);
    }
    else
    {
        std::cerr << "StatusNotifierWatcher: unknown method " << method_name << std::endl;
    }
}

void Watcher::on_interface_get_property(Glib::VariantBase &property,
                                        const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                        const Glib::ustring &sender, const Glib::ustring &object_path,
                                        const Glib::ustring &interface_name, const Glib::ustring &property_name)
{
    if (property_name == "RegisteredStatusNotifierItems")
    {
        std::vector<Glib::ustring> sn_items_names(sn_items.size());
        std::transform(sn_items.begin(), sn_items.end(), sn_items_names.begin(),
                       [](const auto &proxy) { return proxy->get_name(); });
        property = Glib::Variant<std::vector<Glib::ustring>>::create(sn_items_names);
    }
    else if (property_name == "IsStatusNotifierHostRegistered")
    {
        property = Glib::Variant<bool>::create(!sn_hosts.empty());
    }
    else if (property_name == "ProtocolVersion")
    {
        property = Glib::Variant<int>::create(0);
    }
    else
    {
        std::cerr << "StatusNotifierWatcher: Unknown property " << property_name << std::endl;
    }
}
