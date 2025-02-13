#ifndef TRAY_ITEM_HPP
#define TRAY_ITEM_HPP

#include <giomm.h>
#include <gtkmm.h>

#include <wf-option-wrap.hpp>
#include <libdbusmenu-glib/dbusmenu-glib.h>
#include "dbusmenu.hpp"
#include <sstream>
#include <string>


#include <optional>

class StatusNotifierItem : public Gtk::MenuButton
{
    guint menu_handler_id;

    WfOption<int> smooth_scolling_threshold{"panel/tray_smooth_scrolling_threshold"};
    WfOption<bool> menu_on_middle_click{"panel/tray_menu_on_middle_click"};

    Glib::ustring dbus_name, menu_path;

    Glib::RefPtr<Gio::DBus::Proxy> item_proxy;

    std::shared_ptr<DbusMenuModel> menu;

    Gtk::Image icon;

    gdouble distance_scrolled_x = 0;
    gdouble distance_scrolled_y = 0;


    Glib::RefPtr<Gtk::IconTheme> icon_theme;

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

    void fetch_property(const Glib::ustring & property_name, const sigc::slot<void()> & callback = {});

    std::string dbus_name_as_prefix();

  public:
    void menu_update(DbusmenuClient * client);
    explicit StatusNotifierItem(const Glib::ustring & service);
    ~StatusNotifierItem();
    std::string get_unique_name();
};

#endif
