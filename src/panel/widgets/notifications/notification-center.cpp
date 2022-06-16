#include "notification-center.hpp"
#include "widgets/notifications/single-notification.hpp"
#include <iostream>

void WayfireNotificationCenter::init(Gtk::HBox *container)
{
    Daemon::connect(this);
    button = std::make_unique<WayfireMenuButton>("panel");

    icon.set_from_icon_name("notification", Gtk::ICON_SIZE_LARGE_TOOLBAR);
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
}

/*!
 * This function should be called only by Daemon when it receives a new notification.
 */
void WayfireNotificationCenter::newNotification(Notification::id_type id)
{
    const auto &notification = Daemon::getNotifications().at(id);
    g_assert(notification_widgets.count(id) == 0);
    notification_widgets.insert({id, std::make_unique<WfSingleNotification>(notification)});
    auto &widget = notification_widgets.at(id);
    vbox.pack_end(*widget);
    vbox.show_all();
    widget->set_reveal_child();
    button->get_popover()->popup();
}

/*!
 * This function should be called only by the Daemon when the notification is closed.
 *
 * Removes widget of the notification.
 */
void WayfireNotificationCenter::removeNotification(Notification::id_type id)
{
    auto &widget = notification_widgets.at(id);
    widget->property_child_revealed().signal_changed().connect([=] {
        notification_widgets.erase(id);
    });
    widget->set_reveal_child(false);
}

void WayfireNotificationCenter::replaceNotification(Notification::id_type id)
{
    auto &widget = notification_widgets.at(id);
    std::cout << widget->property_reveal_child().get_value() << "\n";
    widget->property_child_revealed().signal_changed().connect([=] {
        notification_widgets.erase(id);
        newNotification(id);
    });
    widget->set_reveal_child(false);
}

void WayfireNotificationCenter::onDaemonStop()
{
    button->get_popover()->remove();
    button->get_popover()->add(status_label);
}
