#include "single-notification.hpp"
#include "daemon.hpp"

#include <glibmm/main.h>
#include <gtk-utils.hpp>
#include <gtkmm.h>

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
        app_icon.set(path_from_uri(notification.app_icon));
    } else
    {
        app_icon.set_from_icon_name(notification.app_icon);
    }

    get_style_context()->add_class("notification");

    app_icon.get_style_context()->add_class("app-icon");

    top_bar.get_style_context()->add_class("top-bar");
    top_bar.append(app_icon);

    app_name.set_label(notification.app_name);
    app_name.set_halign(Gtk::Align::START);
    app_name.set_ellipsize(Pango::EllipsizeMode::END);
    app_name.get_style_context()->add_class("app-name");
    top_bar.append(app_name);


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
    top_bar.append(time_label);

    close_image.set_from_icon_name("window-close");
    close_button.set_child(close_image);
    close_button.get_style_context()->add_class("flat");
    close_button.get_style_context()->add_class("close");
    close_button.signal_clicked().connect(
        [=] { Daemon::Instance()->closeNotification(notification.id, Daemon::CloseReason::Dismissed); });
    top_bar.append(close_button);
    top_bar.set_spacing(5);

    child.append(top_bar);

    if (notification.hints.image_data)
    {
        int width;
        int height;
        auto image_pixbuf = notification.hints.image_data;

        image.set(image_pixbuf);
    } else if (!notification.hints.image_path.empty())
    {
        if (is_file_uri(notification.hints.image_path))
        {
            image.set(path_from_uri(notification.hints.image_path));
        } else
        {
            image.set_from_icon_name(notification.hints.image_path);
        }
    }

    content.get_style_context()->add_class("notification-contents");
    content.append(image);

    text.set_halign(Gtk::Align::START);
    text.set_wrap();
    text.set_wrap_mode(Pango::WrapMode::CHAR);
    if (notification.body.empty())
    {
        text.set_markup(notification.summary);
    } else
    {
        // NOTE: that is not a really right way to implement FDN markup feature, but the easiest one.
        text.set_markup("<b>" + notification.summary + "</b>" + "\n" + notification.body);
    }

    content.append(text);

    child.append(content);

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
                actions.append(*action_button.get());
            } else
            {
                auto click_gesture = Gtk::GestureClick::create();
                click_gesture->signal_pressed().connect(
                    [id = notification.id, action_key] (int count, double x, double y)
                {
                    Daemon::Instance()->invokeAction(id, action_key);
                });
                default_action_ev_box.add_controller(click_gesture);
            }
        }

        if (!actions.get_children().empty())
        {
            actions.set_homogeneous();
            child.append(actions);
        }
    }

    default_action_ev_box.set_child(child);
    set_child(default_action_ev_box);
    set_transition_type(Gtk::RevealerTransitionType::SLIDE_UP);
}

WfSingleNotification::~WfSingleNotification()
{
    time_label_update.disconnect();
}