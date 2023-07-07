#include "item.hpp"
#include <libdbusmenu-gtk/dbusmenu-gtk.h>

static std::pair<Glib::ustring, Glib::ustring> name_and_obj_path(const Glib::ustring &service)
{
    const auto slash_ind = service.find('/');
    if (slash_ind != Glib::ustring::npos)
    {
        return {service.substr(0, slash_ind), service.substr(slash_ind)};
    }
    return {service, "/StatusNotifierItem"};
}

StatusNotifierItem::StatusNotifierItem(const Glib::ustring &service)
{
    const auto &[name, path] = name_and_obj_path(service);
    dbus_name = name;
    Gio::DBus::Proxy::create_for_bus(Gio::DBus::BUS_TYPE_SESSION, name, path, "org.kde.StatusNotifierItem",
                                     [this](const Glib::RefPtr<Gio::AsyncResult> &result) {
                                         item_proxy = Gio::DBus::Proxy::create_for_bus_finish(result);
                                         for (const auto &property_name : item_proxy->get_cached_property_names())
                                         {
                                             Glib::VariantBase property_variant;
                                             item_proxy->get_cached_property(property_variant, property_name);
                                             item_properties.emplace(property_name, property_variant);
                                         }
                                         init_widget();
                                         /*
                                         item_proxy->call(
                                         "org.freedesktop.DBus.Properies.GetAll",
                                         [this](const Glib::RefPtr<Gio::AsyncResult> &result) {
                                             const auto all_properties = item_proxy->call_finish(result);
                                             Glib::Variant<decltype(item_properties)> variant;
                                             all_properties.get_child(variant);
                                             item_properties = variant.get();
                                             init_widget();
                                         },
                                         Glib::Variant<std::tuple<Glib::ustring>>::create({"org.kde.StatusNotifierItem"}));
                                         */
                                     });
}

using IconData = std::vector<std::tuple<gint32, gint32, std::vector<guint8>>>;

void StatusNotifierItem::init_widget()
{
    add(icon);
    icon.show();
    update_icon();
    init_menu();

    signal_button_press_event().connect([this](GdkEventButton *ev) -> bool {
        const auto ev_coords = Glib::Variant<std::tuple<int, int>>::create({ev->x, ev->y});
        if (((get_item_property<bool>("ItemIsMenu") && ev->button == GDK_BUTTON_SECONDARY) ||
             ev->button == GDK_BUTTON_MIDDLE))
        {
            if (menu)
            {
                menu->popup_at_pointer((GdkEvent *)ev);
            }
            else
            {
                item_proxy->call("ContextMenu", ev_coords);
            }
        }
        else if (ev->button == GDK_BUTTON_PRIMARY)
        {
            item_proxy->call("Activate", ev_coords);
        }
        else if (ev->button == GDK_BUTTON_SECONDARY)
        {
            item_proxy->call("SecondaryActivate", ev_coords);
        }
        return true;
    });

    signal_scroll_event().connect([this](GdkEventScroll *ev) {
        static constexpr auto SMOOTH_SCROLL_THRESHOLD = 5.0; // TODO(NamorNiradnug) make this value configurable
        int dx = 0;
        int dy = 0;
        switch (ev->direction)
        {
        case GDK_SCROLL_UP:
            dy = -1;
            break;
        case GDK_SCROLL_DOWN:
            dy = 1;
            break;
        case GDK_SCROLL_LEFT:
            dx = -1;
            break;
        case GDK_SCROLL_RIGHT:
            dx = 1;
            break;
        case GDK_SCROLL_SMOOTH:
            distance_scrolled_x += ev->delta_x;
            distance_scrolled_y += ev->delta_y;
            if (std::abs(distance_scrolled_x) > SMOOTH_SCROLL_THRESHOLD)
            {
                dx = std::lround(distance_scrolled_x);
                distance_scrolled_x = 0;
            }
            if (distance_scrolled_y > SMOOTH_SCROLL_THRESHOLD)
            {
                dy = std::lround(distance_scrolled_y);
            }
            break;
        }

        using ScrollParams = Glib::Variant<std::tuple<int, Glib::ustring>>;
        if (dx != 0)
        {
            item_proxy->call("Scroll", ScrollParams::create({dx, "hozirontal"}));
        }
        if (dy != 0)
        {
            item_proxy->call("Scroll", ScrollParams::create({dy, "vertical"}));
        }
        return true;
    });

    const auto &[tooltip_icon_name, tooltip_icon_data, tooltip_title, tooltip_text] =
        get_item_property<std::tuple<Glib::ustring, IconData, Glib::ustring, Glib::ustring>>("ToolTip");
    if (!tooltip_text.empty())
    {
        set_tooltip_markup(tooltip_text);
    }
    else if (!tooltip_title.empty())
    {
        set_tooltip_markup(tooltip_title);
    }
}

Glib::RefPtr<Gdk::Pixbuf> extract_pixbuf(IconData &&pixbuf_data)
{
    if (pixbuf_data.empty())
    {
        return {};
    }
    auto chosen_image = std::max_element(pixbuf_data.begin(), pixbuf_data.end());
    auto &[width, height, data] = *chosen_image;
    /* argb to rgba */
    for (size_t i = 0; i + 3 < data.size(); i += 4)
    {
        const auto alpha = data[i];
        data[i] = data[i + 1];
        data[i + 1] = data[i + 2];
        data[i + 2] = data[i + 3];
        data[i + 3] = alpha;
    }
    auto *data_ptr = new auto(std::move(data));
    return Gdk::Pixbuf::create_from_data(data_ptr->data(), Gdk::Colorspace::COLORSPACE_RGB, true, 8, width, height,
                                         4 * width, [data_ptr](auto *) { delete data_ptr; });
}

void StatusNotifierItem::update_icon()
{
    const Glib::ustring icon_type_name =
        get_item_property<Glib::ustring>("Status") == "NeedsAttention" ? "AttentionIcon" : "Icon";
    const auto pixmap_data = extract_pixbuf(get_item_property<IconData>(icon_type_name + "Pixmap"));
    if (pixmap_data)
    {
        icon.set(pixmap_data);
    }
    else
    {
        icon.set_from_icon_name(get_item_property<Glib::ustring>(icon_type_name + "Name"),
                                Gtk::ICON_SIZE_LARGE_TOOLBAR);
    }
}

void StatusNotifierItem::init_menu()
{
    const auto menu_path = get_item_property<Glib::DBusObjectPathString>("Menu");
    if (menu_path.empty())
    {
        return;
    }
    auto *raw_menu = dbusmenu_gtkmenu_new((gchar *)dbus_name.data(), (gchar *)menu_path.data());
    if (raw_menu == nullptr)
    {
        return;
    }
    menu = std::move(*Glib::wrap(GTK_MENU(raw_menu)));
    menu->attach_to_widget(*this);
}
