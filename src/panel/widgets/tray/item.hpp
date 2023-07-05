#ifndef TRAY_ITEM_HPP
#define TRAY_ITEM_HPP

#include "gtkmm/image.h"
#include "gtkmm/menu.h"

#include <giomm.h>

class StatusNotifierItem : public Gtk::Image
{
    Glib::RefPtr<Gio::DBus::Proxy> item_proxy;
    Gtk::Menu menu;

    std::map<Glib::ustring, Glib::VariantBase> item_properties;

    template <typename T>
    T get_item_property(const Glib::ustring &name) const
    {
        if (item_properties.count(name) == 0)
        {
            return T();
        }
        return Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(item_properties.at(name)).get();
    }

    void init_widget();
    void update_icon();

    public:
    explicit StatusNotifierItem(const Glib::ustring &service);
};

#endif
