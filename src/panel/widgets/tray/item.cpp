#include "item.hpp"

#include <gtk-utils.hpp>

#include <gtkmm/icontheme.h>
#include <gtkmm/tooltip.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib-object.h>
#include <giomm/dbusmenumodel.h>

#include <cassert>

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
        data_ptr->data(), Gdk::Colorspace::RGB, true, 8, width, height,
        4 * width, [data_ptr] (auto*) { delete data_ptr; });
}

StatusNotifierItem::StatusNotifierItem(const Glib::ustring & service)
{
    set_child(icon);
    menu = std::make_shared<DbusMenuModel>();

    const auto & [name, path] = name_and_obj_path(service);
    dbus_name = name;
    Gio::DBus::Proxy::create_for_bus(
        Gio::DBus::BusType::SESSION, name, path, "org.kde.StatusNotifierItem",
        [this] (const Glib::RefPtr<Gio::AsyncResult> & result)
    {
        item_proxy = Gio::DBus::Proxy::create_for_bus_finish(result);
        signals.push_back(item_proxy->signal_signal().connect(
            [this] (const Glib::ustring & sender, const Glib::ustring & signal,
                    const Glib::VariantContainerBase & params) { handle_signal(signal, params); }));
        init_widget();
    });
}

StatusNotifierItem::~StatusNotifierItem()
{
    gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void StatusNotifierItem::init_widget()
{
    update_icon();
    setup_tooltip();
    init_menu();
    add_css_class("widget-icon");
    add_css_class("tray-button");
    add_css_class("flat");
    gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(gobj()));

    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
    signals.push_back(scroll_gesture->signal_scroll().connect([=] (double dx, double dy) -> bool
    {
        using ScrollParams = Glib::Variant<std::tuple<int, Glib::ustring>>;
        item_proxy->call("Scroll", ScrollParams::create({dx, "horizontal"}));
        item_proxy->call("Scroll", ScrollParams::create({dy, "vertical"}));
        return true;
    }, true));

    auto click_gesture = Gtk::GestureClick::create();
    auto long_press    = Gtk::GestureLongPress::create();
    long_press->set_touch_only(true);
    long_press->signal_pressed().connect(
        [=] (double x, double y)
    {
        popover.popup();
        long_press->set_state(Gtk::EventSequenceState::CLAIMED);
        click_gesture->set_state(Gtk::EventSequenceState::DENIED);
    });
    click_gesture->set_button(0);
    signals.push_back(click_gesture->signal_pressed().connect([=] (int count, double x, double y)
    {
        click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
        return;
    }));
    signals.push_back(click_gesture->signal_released().connect([=] (int count, double x, double y)
    {
        int butt = click_gesture->get_current_button();
        const auto ev_coords = Glib::Variant<std::tuple<int, int>>::create({0, 0});

        const int primary_click   = 1;
        const int secondary_click = menu_on_middle_click ? 2 : 3;
        const int tertiary_click  = menu_on_middle_click ? 3 : 2;
        if (butt == primary_click)
        {
            if (get_item_property<bool>("ItemIsMenu", true))
            {
                if (has_menu)
                {
                    popover.popup();
                } else
                {
                    item_proxy->call("ContextMenu", ev_coords);
                }
            } else
            {
                item_proxy->call("Activate", ev_coords);
            }
        } else if (butt == secondary_click)
        {
            if (has_menu)
            {
                popover.popup();
            } else
            {
                item_proxy->call("ContextMenu", ev_coords);
            }
        } else if (butt == tertiary_click)
        {
            item_proxy->call("SecondaryActivate", ev_coords);
        }

        return;
    }));
    add_controller(long_press);
    add_controller(scroll_gesture);
    add_controller(click_gesture);
}

void StatusNotifierItem::setup_tooltip()
{
    set_has_tooltip();
    signals.push_back(signal_query_tooltip().connect([this] (int, int, bool,
                                                             const std::shared_ptr<Gtk::Tooltip> & tooltip)
    {
        auto [tooltip_icon_name, tooltip_icon_data, tooltip_title, tooltip_text] =
            get_item_property<std::tuple<Glib::ustring, IconData, Glib::ustring, Glib::ustring>>("ToolTip");

        auto tooltip_label_text = !tooltip_text.empty() && !tooltip_title.empty() ?
            "<b>" + tooltip_title + "</b>: " + tooltip_text :
            !tooltip_title.empty() ? tooltip_title :
            !tooltip_text.empty() ? tooltip_text :
            get_item_property<Glib::ustring>("Title");

        const auto pixbuf = extract_pixbuf(std::move(tooltip_icon_data));
        bool icon_shown   = false;
        if (pixbuf)
        {
            tooltip->set_icon(pixbuf);
            icon_shown = true;
        } else
        {
            // tooltip->set_icon_from_name(tooltip_icon_name);
        }

        tooltip->set_markup(tooltip_label_text);
        return icon_shown || !tooltip_label_text.empty();
    }, true));
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
        icon_theme = Gtk::IconTheme::get_for_display(get_display());
    }

    const Glib::ustring icon_type_name =
        get_item_property<Glib::ustring>("Status") == "NeedsAttention" ? "AttentionIcon" : "Icon";
    const bool hide = get_item_property<Glib::ustring>("Status") == "Passive";
    const auto icon_name   = get_item_property<Glib::ustring>(icon_type_name + "Name");
    const auto pixmap_data = extract_pixbuf(get_item_property<IconData>(icon_type_name + "Pixmap"));
    if (pixmap_data)
    {
        icon.set(pixmap_data);
    } else
    {
        icon.set_from_icon_name(icon_name);
    }

    if (hide)
    {
        this->hide();
    } else
    {
        this->show();
    }
}

void StatusNotifierItem::init_menu()
{
    menu_path = get_item_property<Glib::DBusObjectPathString>("Menu");

    if (menu_path.empty())
    {
        return;
    }

    auto action_prefix = dbus_name_as_prefix();

    menu->connect(dbus_name, menu_path, action_prefix);
    signals.push_back(menu->signal_action_group().connect([=] ()
    {
        auto action_group = menu->get_action_group();
        insert_action_group(action_prefix, action_group);
        popover.set_menu_model(menu->get_menu());
    }));
    has_menu = true;
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
    const sigc::slot<void()> & callback)
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

std::string StatusNotifierItem::get_unique_name()
{
    std::stringstream ss;
    ss << dbus_name << "_" << menu_path;
    return ss.str();
}

/*
 *  DBUS names are in the format of :1.61
 *  I have no idea what this means, I frankly don't care, but I need a way to generate an acceptable action
 * group name from this
 *  such that it always gets the same unique output for the same input
 */

const std::string CHARS_IN  = ":0123456789.";
const std::string CHARS_OUT = "zabcdefghijk";
std::string StatusNotifierItem::dbus_name_as_prefix()
{
    std::unordered_map<char, char> map;
    for (int i = 0; i < (int)CHARS_IN.length(); i++)
    {
        map[CHARS_IN[i]] = CHARS_OUT[i];
    }

    std::stringstream ss;
    for (int i = 0; i < (int)dbus_name.length(); i++)
    {
        ss << map[dbus_name[i]];
    }

    return ss.str();
}
