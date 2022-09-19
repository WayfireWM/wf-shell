#include "notification-center.hpp"

#include <glibmm/main.h>

#include "daemon.hpp"
#include "single-notification.hpp"

void WayfireNotificationCenter::init(Gtk::HBox *container)
{
    Daemon::start();
    Daemon::signalNotificationNew().connect([=](Notification::id_type id) { newNotification(id); });
    Daemon::signalNotificationReplaced().connect([=](Notification::id_type id) { replaceNotification(id); });
    Daemon::signalNotificationClosed().connect([=](Notification::id_type id) { closeNotification(id); });
    Daemon::signalDaemonStopped().connect([=] { onDaemonStop(); });

    button = std::make_unique<WayfireMenuButton>("panel");

    updateIcon();
    button->add(icon);
    container->add(*button);

    auto *popover = button->get_popover();
    popover->set_size_request(WIDTH, HEIGHT);

    vbox.set_valign(Gtk::ALIGN_START);
    scrolled_window.add(vbox);
    scrolled_window.show_all();
    popover->add(scrolled_window);

    status_label.show();
    status_label.set_line_wrap();
    status_label.set_line_wrap_mode(Pango::WRAP_WORD);

    button->set_tooltip_text("Middle click to toggle DND mode.");
    button->signal_button_press_event().connect_notify([=](GdkEventButton *ev) {
        if (ev->button == 2)
        {
            dnd_enabled = !dnd_enabled;
            updateIcon();
        }
    });
}

void WayfireNotificationCenter::newNotification(Notification::id_type id)
{
    const auto &notification = Daemon::getNotifications().at(id);
    g_assert(notification_widgets.count(id) == 0);
    notification_widgets.insert({id, std::make_unique<WfSingleNotification>(notification)});
    auto &widget = notification_widgets.at(id);
    vbox.pack_end(*widget);
    vbox.show_all();
    widget->set_reveal_child();
    if (!dnd_enabled || (show_critical_in_dnd && Daemon::getNotifications().at(id).hints.urgency == 2))
    {
        auto *popover = button->get_popover();
        if (timeout > 0 && (!popover_timeout.empty() || !popover->is_visible()))
        {
            popover_timeout.disconnect();
            popover_timeout = Glib::signal_timeout().connect(
                [=] {
                    popover->popdown();
                    popover_timeout.disconnect();
                    return true;
                },
                timeout * 1000);
        }
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
    auto &widget = notification_widgets.at(id);
    widget->property_child_revealed().signal_changed().connect([=] {
        notification_widgets.erase(id);
        newNotification(id);
    });
    widget->set_reveal_child(false);
}

void WayfireNotificationCenter::closeNotification(Notification::id_type id)
{
    if (notification_widgets.count(id) == 0)
        return;
    auto &widget = notification_widgets.at(id);
    widget->property_child_revealed().signal_changed().connect([=] { notification_widgets.erase(id); });
    widget->set_reveal_child(false);
}

void WayfireNotificationCenter::onDaemonStop()
{
    button->get_popover()->remove();
    button->get_popover()->add(status_label);
}

void WayfireNotificationCenter::updateIcon()
{
    if (dnd_enabled)
        icon.set_from_icon_name("notifications-disabled", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    else
        icon.set_from_icon_name("notifications", Gtk::ICON_SIZE_LARGE_TOOLBAR);
}
