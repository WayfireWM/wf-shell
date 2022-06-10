#ifndef WIDGETS_SINGLE_NOTIFICATION_HPP
#define WIDGETS_SINGLE_NOTIFICATION_HPP

#include <gdkmm/pixbuf.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include "notification-info.hpp"

class WfSingleNotification : public Gtk::VBox
{
    public:
    static const int WIDTH = 300;

    private:
    Gtk::HBox top_bar;
    Gtk::HBox content;

    Gtk::Image app_icon;
    Gtk::Label app_name;
    Gtk::Button close_button;
    Gtk::Image close_image;

    Gtk::Label text;
    Gtk::Image image;

    public:
    explicit WfSingleNotification(const Notification &notification);
};

#endif

