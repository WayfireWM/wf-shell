#include "notification-center.hpp"

#include <glibmm/main.h>

#include <gtk-utils.hpp>

#include "single-notification.hpp"

void WayfireNotificationCenter::init(Gtk::HBox *container)
{
    button = std::make_unique<WayfireMenuButton>("panel");

    updateIcon();
    button->add(icon);
    container->add(*button);
    button->show_all();

    auto *popover = button->get_popover();
    popover->set_size_request(WIDTH, HEIGHT);

    vbox.set_valign(Gtk::ALIGN_START);
    scrolled_window.add(vbox);
    scrolled_window.show_all();
    popover->add(scrolled_window);

    button->set_tooltip_text("Middle click to toggle DND mode.");
    button->signal_button_press_event().connect_notify([=] (GdkEventButton *ev)
    {
        if (ev->button == 2)
        {
            dnd_enabled = !dnd_enabled;
            updateIcon();
        }
    });

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
    vbox.pack_end(*widget);
    vbox.show_all();
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
        set_image_icon(icon, "notifications-disabled", icon_size);
    } else
    {
        set_image_icon(icon, "notifications", icon_size);
    }
}
