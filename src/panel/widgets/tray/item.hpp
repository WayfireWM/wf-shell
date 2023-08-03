#ifndef TRAY_ITEM_HPP
#define TRAY_ITEM_HPP

#include <giomm.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/menu.h>

#include <wf-option-wrap.hpp>

#include <optional>

class StatusNotifierItem : public Gtk::EventBox
{
    WfOption<int> smooth_scolling_threshold{"panel/tray_smooth_scrolling_threshold"};
    WfOption<int> icon_size{"panel/tray_icon_size"};
    WfOption<bool> menu_on_middle_click{"panel/tray_menu_on_middle_click"};

    Glib::ustring dbus_name;

    Glib::RefPtr<Gio::DBus::Proxy> item_proxy;

    Gtk::Image icon;
    std::optional<Gtk::Menu> menu;

    gdouble distance_scrolled_x = 0;
    gdouble distance_scrolled_y = 0;

    Glib::RefPtr<Gtk::IconTheme> icon_theme = Gtk::IconTheme::get_default();

    template<typename T>
    T get_item_property(const Glib::ustring & name, const T & default_value = {}) const
    {
        Glib::VariantBase variant;
        item_proxy->get_cached_property(variant, name);
        return variant && variant.is_of_type(Glib::Variant<T>::variant_type()) ?
               Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(variant).get() :
               default_value;
    }

    void init_widget();
    void init_menu();

    void handle_signal(const Glib::ustring & signal, const Glib::VariantContainerBase & params);

    void update_icon();
    void setup_tooltip();

    void fetch_property(const Glib::ustring & property_name, const sigc::slot<void> & callback = {});

  public:
    explicit StatusNotifierItem(const Glib::ustring & service);
};

#endif
