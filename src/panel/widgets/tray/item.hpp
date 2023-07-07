#ifndef TRAY_ITEM_HPP
#define TRAY_ITEM_HPP

#include "gtkmm/eventbox.h"
#include "gtkmm/image.h"
#include "gtkmm/menu.h"

#include <giomm.h>
#include <optional>

class StatusNotifierItem : public Gtk::EventBox
{
    Glib::ustring dbus_name;

    Glib::RefPtr<Gio::DBus::Proxy> item_proxy;

    Gtk::Image icon;
    std::optional<Gtk::Menu> menu;

    gdouble distance_scrolled_x = 0;
    gdouble distance_scrolled_y = 0;

    std::map<Glib::ustring, Glib::VariantBase> item_properties;

    template <typename T>
    T get_item_property(const Glib::ustring &name) const
    {
        if (item_properties.count(name) == 0)
        {
            return {};
        }
        if (!item_properties.at(name).is_of_type(Glib::Variant<T>::variant_type()))
        {
            return {};
        }
        return Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(item_properties.at(name)).get();
    }

    void init_widget();
    void update_icon();
    void init_menu();

    public:
    explicit StatusNotifierItem(const Glib::ustring &service);
};

#endif
