#include "notification-center.hpp"

#include <glibmm/main.h>
#include <gtkmm.h>

#include <gtk-utils.hpp>

#include "single-notification.hpp"

void WayfireNotificationCenter::init(Gtk::Box *container)
{
    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("notification-center");
    button->get_children()[0]->get_style_context()->add_class("flat");

    updateIcon();
    button->set_child(icon);
    container->append(*button);

    auto *popover = button->get_popover();
    popover->set_size_request(WIDTH, HEIGHT);
    popover->get_style_context()->add_class("notification-popover");

    box.set_valign(Gtk::Align::START);
    box.set_orientation(Gtk::Orientation::VERTICAL);
    scrolled_window.set_child(box);
    popover->set_child(scrolled_window);

    button->set_tooltip_text("Middle click to toggle DND mode.");

    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_button(2);
    click_gesture->signal_pressed().connect([=] (int count, double x, double y)
    {
        dnd_enabled = !dnd_enabled;
        updateIcon();
        click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
    });
    button->add_controller(click_gesture);

    for (const auto & [id, _] : daemon->getNotifications())
    {
        newNotification(id, false);
    }

    notification_new_conn =
        daemon->signalNotificationNew().connect([=] (Notification::id_type id) { newNotification(id); });
    notification_replace_conn =
        daemon->signalNotificationReplaced().connect([=] (Notification::id_type id)
    {
        replaceNotification(id);
    });
    notification_close_conn =
        daemon->signalNotificationClosed().connect([=] (Notification::id_type id) { closeNotification(id); });
}

void WayfireNotificationCenter::newNotification(Notification::id_type id, bool show_popup)
{
    const auto & notification = daemon->getNotifications().at(id);
    g_assert(notification_widgets.count(id) == 0);
    notification_widgets.insert({id, std::make_unique<WfSingleNotification>(notification)});
    auto & widget = notification_widgets.at(id);
    box.append(*widget);
    widget->set_reveal_child();
    if (show_popup && !dnd_enabled || (show_critical_in_dnd && (notification.hints.urgency == 2)))
    {
        auto *popover = button->get_popover();
        if ((timeout > 0) && (!popover_timeout.empty() || !popover->is_visible()))
        {
            popover_timeout.disconnect();
            popover_timeout = Glib::signal_timeout().connect(
                [=]
            {
                popover->popdown();
                button->set_keyboard_interactive();
                popover_timeout.disconnect();
                return true;
            },
                timeout * 1000);
        }

        button->set_keyboard_interactive(false);
        popover->popup();
    }
}

void WayfireNotificationCenter::replaceNotification(Notification::id_type id)
{
    if (notification_widgets.count(id) == 0)
    {
        newNotification(id);
        return;
    }

    auto & widget = notification_widgets.at(id);
    widget->property_child_revealed().signal_changed().connect([=]
    {
        notification_widgets.erase(id);
        newNotification(id);
    });
    widget->set_reveal_child(false);
}

void WayfireNotificationCenter::closeNotification(Notification::id_type id)
{
    if (notification_widgets.count(id) == 0)
    {
        return;
    }

    auto & widget = notification_widgets.at(id);
    widget->property_child_revealed().signal_changed().connect([=] { notification_widgets.erase(id); });
    widget->set_reveal_child(false);
}

void WayfireNotificationCenter::updateIcon()
{
    if (dnd_enabled)
    {
        icon.set_from_icon_name("notifications-disabled");
    } else
    {
        icon.set_from_icon_name("notifications");
    }
}
