#include "item.hpp"

#include <gtk-utils.hpp>

#include <gtkmm/icontheme.h>
#include <gtkmm/tooltip.h>

#include <libdbusmenu-gtk/dbusmenu-gtk.h>

static std::pair<Glib::ustring, Glib::ustring> name_and_obj_path(const Glib::ustring & service)
{
    const auto slash_ind = service.find('/');
    if (slash_ind != Glib::ustring::npos)
    {
        return {service.substr(0, slash_ind), service.substr(slash_ind)};
    }

    return {service, "/StatusNotifierItem"};
}

using IconData = std::vector<std::tuple<gint32, gint32, std::vector<guint8>>>;

static Glib::RefPtr<Gdk::Pixbuf> extract_pixbuf(IconData && pixbuf_data)
{
    if (pixbuf_data.empty())
    {
        return {};
    }

    auto chosen_image = std::max_element(pixbuf_data.begin(), pixbuf_data.end());
    auto & [width, height, data] = *chosen_image;
    /* argb to rgba */
    for (size_t i = 0; i + 3 < data.size(); i += 4)
    {
        const auto alpha = data[i];
        data[i]     = data[i + 1];
        data[i + 1] = data[i + 2];
        data[i + 2] = data[i + 3];
        data[i + 3] = alpha;
    }

    auto *data_ptr = new auto(std::move(data));
    return Gdk::Pixbuf::create_from_data(
        data_ptr->data(), Gdk::Colorspace::COLORSPACE_RGB, true, 8, width, height,
        4 * width, [data_ptr] (auto*) { delete data_ptr; });
}

StatusNotifierItem::StatusNotifierItem(const Glib::ustring & service)
{
    add(icon);

    const auto & [name, path] = name_and_obj_path(service);
    dbus_name = name;
    Gio::DBus::Proxy::create_for_bus(
        Gio::DBus::BUS_TYPE_SESSION, name, path, "org.kde.StatusNotifierItem",
        [this] (const Glib::RefPtr<Gio::AsyncResult> & result)
    {
        item_proxy = Gio::DBus::Proxy::create_for_bus_finish(result);
        item_proxy->signal_signal().connect(
            [this] (const Glib::ustring & sender, const Glib::ustring & signal,
                    const Glib::VariantContainerBase & params) { handle_signal(signal, params); });
        init_widget();
    });
}

void StatusNotifierItem::init_widget()
{
    update_icon();
    icon_size.set_callback([this] { update_icon(); });
    setup_tooltip();
    init_menu();

    signal_button_press_event().connect([this] (GdkEventButton *ev) -> bool
    {
        const auto ev_coords = Glib::Variant<std::tuple<int, int>>::create({ev->x, ev->y});
        const guint menu_btn = menu_on_middle_click ? GDK_BUTTON_MIDDLE : GDK_BUTTON_SECONDARY;
        const guint secondary_activate_btn = menu_on_middle_click ? GDK_BUTTON_SECONDARY : GDK_BUTTON_MIDDLE;
        if (get_item_property<bool>("ItemIsMenu", true) || (ev->button == menu_btn))
        {
            if (menu)
            {
                menu->popup_at_pointer((GdkEvent*)ev);
            } else
            {
                item_proxy->call("ContextMenu", ev_coords);
            }
        } else if (ev->button == GDK_BUTTON_PRIMARY)
        {
            item_proxy->call("Activate", ev_coords);
        } else if (ev->button == secondary_activate_btn)
        {
            item_proxy->call("SecondaryActivate", ev_coords);
        }

        return true;
    });

    add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);

    signal_scroll_event().connect([this] (GdkEventScroll *ev)
    {
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
            if (std::abs(distance_scrolled_x) >= smooth_scolling_threshold)
            {
                dx = std::lround(distance_scrolled_x);
                distance_scrolled_x = 0;
            }

            if (std::abs(distance_scrolled_y) >= smooth_scolling_threshold)
            {
                dy = std::lround(distance_scrolled_y);
                distance_scrolled_y = 0;
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
}

void StatusNotifierItem::setup_tooltip()
{
    set_has_tooltip();
    signal_query_tooltip().connect([this] (int, int, bool, const Glib::RefPtr<Gtk::Tooltip> & tooltip)
    {
        auto [tooltip_icon_name, tooltip_icon_data, tooltip_title, tooltip_text] =
            get_item_property<std::tuple<Glib::ustring, IconData, Glib::ustring, Glib::ustring>>("ToolTip");

        auto tooltip_label_text = !tooltip_text.empty() && !tooltip_title.empty() ?
            "<b>" + tooltip_title + "</b>: " + tooltip_text :
            !tooltip_title.empty() ? tooltip_title :
            !tooltip_text.empty() ? tooltip_text :
            get_item_property<Glib::ustring>("Title");

        const auto pixbuf = extract_pixbuf(std::move(tooltip_icon_data));

        bool icon_shown = true;
        if (icon_theme->has_icon(tooltip_icon_name))
        {
            tooltip->set_icon_from_icon_name(tooltip_icon_name, Gtk::ICON_SIZE_LARGE_TOOLBAR);
        } else if (pixbuf)
        {
            tooltip->set_icon(pixbuf);
        } else
        {
            icon_shown = false;
        }

        tooltip->set_markup(tooltip_label_text);
        return icon_shown || !tooltip_label_text.empty();
    });
}

void StatusNotifierItem::update_icon()
{
    if (const auto icon_theme_path = get_item_property<Glib::ustring>(
        "IconThemePath");!icon_theme_path.empty())
    {
        icon_theme = Gtk::IconTheme::create();
        icon_theme->add_resource_path(icon_theme_path);
    } else
    {
        icon_theme = Gtk::IconTheme::get_default();
    }

    const Glib::ustring icon_type_name =
        get_item_property<Glib::ustring>("Status") == "NeedsAttention" ? "AttentionIcon" : "Icon";
    const auto icon_name   = get_item_property<Glib::ustring>(icon_type_name + "Name");
    const auto pixmap_data = extract_pixbuf(get_item_property<IconData>(icon_type_name + "Pixmap"));
    if (icon_theme->lookup_icon(icon_name, icon_size))
    {
        set_image_icon(icon, icon_name, icon_size, {}, icon_theme);
    } else if (pixmap_data)
    {
        icon.set(pixmap_data->scale_simple(icon_size, icon_size, Gdk::INTERP_BILINEAR));
    }
}

void StatusNotifierItem::init_menu()
{
    const auto menu_path = get_item_property<Glib::DBusObjectPathString>("Menu");
    if (menu_path.empty())
    {
        return;
    }

    auto *raw_menu = dbusmenu_gtkmenu_new((gchar*)dbus_name.data(), (gchar*)menu_path.data());
    if (raw_menu == nullptr)
    {
        return;
    }

    menu = std::move(*Glib::wrap(GTK_MENU(raw_menu)));
    menu->attach_to_widget(*this);
}

void StatusNotifierItem::handle_signal(const Glib::ustring & signal,
    const Glib::VariantContainerBase & params)
{
    if (signal.size() < 3)
    {
        return;
    }

    const auto property = signal.substr(3);
    if (property == "ToolTip")
    {
        fetch_property(property);
    } else if (property == "IconThemePath")
    {
        fetch_property(property, [this] { update_icon(); });
    } else if ((property.size() >= 4) && (property.substr(property.size() - 4) == "Icon"))
    {
        fetch_property(property + "Name",
            [this, property] { fetch_property(property + "Pixmap", [this] { update_icon(); }); });
    } else if ((property == "Status") && params.is_of_type(Glib::VariantType("(s)")))
    {
        Glib::Variant<Glib::ustring> status;
        params.get_child(status);
        item_proxy->set_cached_property(property, status);
        update_icon();
    }
}

void StatusNotifierItem::fetch_property(const Glib::ustring & property_name,
    const sigc::slot<void> & callback)
{
    item_proxy->call(
        "org.freedesktop.DBus.Properties.Get",
        [this, property_name, callback] (const Glib::RefPtr<Gio::AsyncResult> & res)
    {
        try {
            auto value = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::VariantBase>>(
                item_proxy->call_finish(res).get_child())
                    .get();
            item_proxy->set_cached_property(property_name, value);
        } catch (const Gio::DBus::Error &)
        {}

        callback();
    },
        Glib::Variant<std::tuple<Glib::ustring, Glib::ustring>>::create({"org.kde.StatusNotifierItem",
            property_name}));
}
