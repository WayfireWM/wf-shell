#include "notification-center.hpp"
#include "widgets/notifications/single-notification.hpp"

void WayfireNotificationCenter::init(Gtk::HBox *container)
{
    Daemon::start(this);
    button = std::make_unique<WayfireMenuButton>("panel");

    icon.set_from_icon_name("notification", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    button->add(icon);
    container->add(*button);

    vbox.set_spacing(5);
    vbox.set_valign(Gtk::ALIGN_START);
    scrolled_window.add(vbox);
    scrolled_window.show_all();

    auto *popover = button->get_popover();
    popover->set_size_request(WIDTH, HEIGHT);
    popover->add(scrolled_window);
}

/*!
 * This function should be called only by Daemon when it receives a new notification.
 */
void WayfireNotificationCenter::newNotification(Notification::id_type id)
{
    const auto notification = Daemon::getNotifications().at(id);
    notification_widgets.insert({id, std::make_unique<WfSingleNotification>(notification)});
    vbox.pack_end(*notification_widgets.at(id));
    vbox.show_all();
    button->get_popover()->popup();
}

/*!
 * This function should be called when the notification is closed by a user or
 * via org.freedesktop.Notifications.CloseNotificationi function.
 */
void WayfireNotificationCenter::closeNotification(Notification::id_type id)
{
}
