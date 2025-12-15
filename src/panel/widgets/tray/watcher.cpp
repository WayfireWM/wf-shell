#include "watcher.hpp"

#include <giomm.h>

#include <iostream>

static const auto introspection_data = Gio::DBus::NodeInfo::create_for_xml(
    R"(
<?xml version="1.0" encoding="UTF-8"?>
<node name="/StatusNotifierWatcher">
    <interface name="org.kde.StatusNotifierWatcher">
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
)")->lookup_interface();

Watcher::Watcher() :
    dbus_name_id(Gio::DBus::own_name(Gio::DBus::BusType::BUS_TYPE_SESSION, SNW_NAME,
        sigc::mem_fun(this, &Watcher::on_bus_acquired)))
{}

std::shared_ptr<Watcher> Watcher::Launch()
{
    if (instance.expired())
    {
        auto watcher_ptr = std::shared_ptr<Watcher>(new Watcher());
        instance = watcher_ptr;
        return watcher_ptr;
    }

    return Instance();
}

std::shared_ptr<Watcher> Watcher::Instance()
{
    return instance.lock();
}

Watcher::~Watcher()
{
    for (const auto& [_, host_id] : sn_hosts_id)
    {
        Gio::DBus::unwatch_name(host_id);
    }

    for (const auto& [_, item_id] : sn_items_id)
    {
        Gio::DBus::unwatch_name(item_id);
    }

    watcher_connection->unregister_object(dbus_object_id);
    Gio::DBus::unown_name(dbus_name_id);
}

void Watcher::on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & name)
{
    dbus_object_id     = connection->register_object(SNW_PATH, introspection_data, interface_table);
    watcher_connection = connection;
}

void Watcher::register_status_notifier_item(const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & sender, const Glib::ustring & path)
{
    const auto full_obj_path = sender + path;
    emit_signal("StatusNotifierItemRegistered", full_obj_path);
    sn_items_id.emplace(full_obj_path, Gio::DBus::watch_name(
        Gio::DBus::BUS_TYPE_SESSION, sender, {},
        [this, full_obj_path] (const Glib::RefPtr<Gio::DBus::Connection> & connection,
                               const Glib::ustring & name)
    {
        Gio::DBus::unwatch_name(sn_items_id.at(full_obj_path));
        sn_items_id.erase(full_obj_path);
        emit_signal("StatusNotifierItemUnregistered", full_obj_path);
    }));
}

void Watcher::register_status_notifier_host(const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & service)
{
    sn_hosts_id.emplace(
        service, Gio::DBus::watch_name(
            connection, service,
            [this, is_host_registred_changed = sn_hosts_id.empty()] (
                const Glib::RefPtr<Gio::DBus::Connection> &, const Glib::ustring &, const Glib::ustring &)
    {
        emit_signal("StatusNotifierHostRegistered");
        if (is_host_registred_changed)
        {
            watcher_connection->emit_signal(
                SNW_PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged", {},
                Glib::Variant<std::tuple<Glib::ustring, std::map<Glib::ustring, Glib::VariantBase>,
                    std::vector<Glib::ustring>>>::
                create({SNW_IFACE,
                    {{"IsStatusNotifierHostRegistered", Glib::Variant<bool>::create(true)}},
                    {}}));
        }
    },
            [this] (const Glib::RefPtr<Gio::DBus::Connection> & connection, const Glib::ustring & name)
    {
        Gio::DBus::unwatch_name(sn_hosts_id[name]);
        sn_hosts_id.erase(name);
    }));
}

void Watcher::on_interface_method_call(const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & sender, const Glib::ustring & object_path,
    const Glib::ustring & interface_name, const Glib::ustring & method_name,
    const Glib::VariantContainerBase & parameters,
    const Glib::RefPtr<Gio::DBus::MethodInvocation> & invocation)
{
    if (!parameters.is_of_type(Glib::VariantType("(s)")))
    {
        std::cerr << "StatusNotifierWatcher: invalid argument type: expected (s), got " <<
            parameters.get_type_string() <<
            std::endl;
        return;
    }

    Glib::Variant<Glib::ustring> service_variant;
    parameters.get_child(service_variant, 0);
    const auto service = service_variant.get();
    if (method_name == "RegisterStatusNotifierItem")
    {
        register_status_notifier_item(connection, service[0] == '/' ? sender : service,
            service[0] == '/' ? service : "/StatusNotifierItem");
    } else if (method_name == "RegisterStatusNotifierHost")
    {
        register_status_notifier_host(connection, service);
    } else
    {
        std::cerr << "StatusNotifierWatcher: unknown method " << method_name << std::endl;
        return;
    }

    invocation->return_value(Glib::VariantContainerBase());
}

void Watcher::on_interface_get_property(Glib::VariantBase & property,
    const Glib::RefPtr<Gio::DBus::Connection> & connection,
    const Glib::ustring & sender, const Glib::ustring & object_path,
    const Glib::ustring & interface_name, const Glib::ustring & property_name)
{
    if (property_name == "RegisteredStatusNotifierItems")
    {
        property = get_registred_items();
    } else if (property_name == "IsStatusNotifierHostRegistered")
    {
        property = Glib::Variant<bool>::create(!sn_hosts_id.empty());
    } else if (property_name == "ProtocolVersion")
    {
        property = Glib::Variant<int>::create(0);
    } else
    {
        std::cerr << "StatusNotifierWatcher: Unknown property " << property_name << std::endl;
    }
}

Glib::Variant<std::vector<Glib::ustring>> Watcher::get_registred_items() const
{
    std::vector<Glib::ustring> sn_items_names;
    sn_items_names.reserve(sn_items_id.size());
    for (const auto & [service, id] : sn_items_id)
    {
        sn_items_names.push_back(service);
    }

    return Glib::Variant<std::vector<Glib::ustring>>::create(sn_items_names);
}
