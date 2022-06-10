#include "single-notification.hpp"
#include <gtk-utils.hpp>
#include <iostream>
#include <string>

const static std::string FILE_PREFIX = "file://";

static bool begins_with(const std::string &str, const std::string &prefix)
{
    return str.size() >= prefix.size() && str.substr(prefix.size()) == prefix;
}

inline static bool is_file_uri(const std::string &str)
{
    return begins_with(str, FILE_PREFIX);
}

inline static std::string path_from_uri(const std::string &str)
{
    return str.substr(FILE_PREFIX.size());
}

WfSingleNotification::WfSingleNotification(const Notification &notification)
{
    if (!notification.app_icon.empty())
    {
        if (is_file_uri(notification.app_icon))
        {
            auto file_name = path_from_uri(notification.app_icon);
            int height = Gtk::IconSize(Gtk::ICON_SIZE_LARGE_TOOLBAR);
            auto pixbuf = load_icon_pixbuf_safe(file_name, height);
            app_icon.set(pixbuf);
        }
        else
        {
            app_icon.set_from_icon_name(notification.app_icon, Gtk::ICON_SIZE_LARGE_TOOLBAR);
        }
        top_bar.pack_start(app_icon);
    }

    app_name.set_label(notification.app_name);
    app_name.set_halign(Gtk::ALIGN_START);
    app_name.set_margin_start(5);
    app_name.set_single_line_mode();
    app_name.set_ellipsize(Pango::ELLIPSIZE_END);
    app_name.set_hexpand();
    top_bar.pack_start(app_name);

    close_image.set_from_icon_name("window-close", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    close_button.add(close_image);
    close_button.get_style_context()->add_class("flat");
    close_button.signal_clicked().connect([=] { std::cout << "Notification with id " << notification.id << " should be closed.\n"; });
    top_bar.pack_start(close_button);

    top_bar.set_spacing(5);

    if (notification.hints.image_data)
    {
        image.set(notification.hints.image_data);
    }
    if (!notification.hints.image_path.empty())
    {
        if (is_file_uri(notification.hints.image_path))
        {
            auto path = path_from_uri(notification.hints.image_path);
            image.set_from_resource(path);
        }
        else
        {
            image.set_from_icon_name(notification.hints.image_path, Gtk::ICON_SIZE_DIALOG);
        }
    }
    content.pack_end(image);

    text.set_halign(Gtk::ALIGN_START);
    text.set_line_wrap();
    text.set_line_wrap_mode(Pango::WRAP_CHAR);
    text.set_markup(notification.summary + "\n" + notification.body);
    content.pack_start(text);

    add(top_bar);
    add(content);
}
