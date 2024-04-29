#include "single-notification.hpp"
#include "daemon.hpp"

#include <glibmm/main.h>
#include <gtk-utils.hpp>
#include <gtkmm/icontheme.h>

#include <ctime>
#include <string>

const static std::string FILE_URI_PREFIX = "file://";

static bool begins_with(const std::string & str, const std::string & prefix)
{
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

inline static bool is_file_uri(const std::string & str)
{
    return begins_with(str, FILE_URI_PREFIX);
}

inline static std::string path_from_uri(const std::string & str)
{
    return str.substr(FILE_URI_PREFIX.size());
}

static const int DAY_SEC = 24 * 60 * 60;

static std::string format_recv_time(const std::time_t & time)
{
    std::time_t delta = std::time(nullptr) - time;
    if (delta > 2 * DAY_SEC)
    {
        return std::to_string(delta / DAY_SEC) + "d ago";
    }

    if (delta > DAY_SEC)
    {
        return "Yesterday";
    }

    char c_str[] = "hh:mm";
    std::strftime(c_str, sizeof(c_str), "%R", std::localtime(&time));
    return std::string(c_str);
}

WfSingleNotification::WfSingleNotification(const Notification & notification)
{
    if (is_file_uri(notification.app_icon))
    {
        auto file_name = path_from_uri(notification.app_icon);
        int height     = Gtk::IconSize(Gtk::ICON_SIZE_LARGE_TOOLBAR);
        auto pixbuf    = load_icon_pixbuf_safe(file_name, height);
        app_icon.set(pixbuf);
    } else if (Gtk::IconTheme::get_default()->has_icon(notification.app_icon))
    {
        app_icon.set_from_icon_name(notification.app_icon, Gtk::ICON_SIZE_LARGE_TOOLBAR);
    } else
    {
        app_icon.set_from_icon_name("dialog-information", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    }

    get_style_context()->add_class("notification");

    app_icon.get_style_context()->add_class("app-icon");

    top_bar.get_style_context()->add_class("top-bar");
    top_bar.pack_start(app_icon, false, true);

    app_name.set_label(notification.app_name);
    app_name.set_halign(Gtk::ALIGN_START);
    app_name.set_ellipsize(Pango::ELLIPSIZE_END);
    app_name.get_style_context()->add_class("app-name");
    top_bar.pack_start(app_name);

    time_label.set_sensitive(false);
    time_label.set_label(format_recv_time(notification.additional_info.recv_time));
    time_label.get_style_context()->add_class("time");
    time_label_update = Glib::signal_timeout().connect(
        [=]
    {
        time_label.set_label(format_recv_time(notification.additional_info.recv_time));
        return true;
    },
        // updating once a day doesn't work with system suspending/hybernating
        10000, Glib::PRIORITY_LOW);
    top_bar.pack_start(time_label, false, true);

    close_image.set_from_icon_name("window-close", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    close_button.add(close_image);
    close_button.get_style_context()->add_class("flat");
    close_button.get_style_context()->add_class("close");
    close_button.signal_clicked().connect(
        [=] { Daemon::Instance()->closeNotification(notification.id, Daemon::CloseReason::Dismissed); });
    top_bar.pack_start(close_button, false, true);

    child.add(top_bar);

    if (notification.hints.image_data)
    {
        image.set(notification.hints.image_data);
    } else if (!notification.hints.image_path.empty())
    {
        if (is_file_uri(notification.hints.image_path))
        {
            image.set(path_from_uri(notification.hints.image_path));
        } else
        {
            image.set_from_icon_name(notification.hints.image_path, Gtk::ICON_SIZE_DIALOG);
        }
    }

    content.get_style_context()->add_class("notification-contents");
    content.pack_end(image);

    text.set_halign(Gtk::ALIGN_START);
    text.set_line_wrap();
    text.set_line_wrap_mode(Pango::WRAP_CHAR);
    if (notification.body.empty())
    {
        text.set_markup(notification.summary);
    } else
    {
        // NOTE: that is not a really right way to implement FDN markup feature, but the easiest one.
        text.set_markup("<b>" + notification.summary + "</b>" + "\n" + notification.body);
    }

    content.pack_start(text);

    child.add(content);

    actions.get_style_context()->add_class("actions");

    if (!notification.actions.empty())
    {
        for (uint i = 0; i + 1 < notification.actions.size(); ++++ i)
        {
            if (const auto action_key = notification.actions[i];action_key != "default")
            {
                auto action_button = Glib::RefPtr<Gtk::Button>(new Gtk::Button(notification.actions[i + 1]));
                action_button->signal_clicked().connect(
                    [id = notification.id, action_key] { Daemon::Instance()->invokeAction(id, action_key); });
                actions.add(*action_button.get());
            } else
            {
                default_action_ev_box.signal_button_press_event().connect(
                    [id = notification.id, action_key] (GdkEventButton *ev)
                {
                    if (ev->button == GDK_BUTTON_PRIMARY)
                    {
                        Daemon::Instance()->invokeAction(id, action_key);
                        return false;
                    }

                    return true;
                });
            }
        }

        if (!actions.get_children().empty())
        {
            actions.set_homogeneous();
            child.add(actions);
        }
    }

    default_action_ev_box.add(child);
    add(default_action_ev_box);
    set_transition_type(Gtk::REVEALER_TRANSITION_TYPE_SLIDE_UP);
}

WfSingleNotification::~WfSingleNotification()
{
    time_label_update.disconnect();
}
