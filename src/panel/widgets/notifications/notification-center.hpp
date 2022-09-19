#ifndef NOTIFICATION_CENTER_HPP
#define NOTIFICATION_CENTER_HPP

#include "../../widget.hpp"
#include "single-notification.hpp"
#include <gtkmm/scrolledwindow.h>
#include <wf-popover.hpp>

class WayfireNotificationCenter : public WayfireWidget
{
    private:
    static const int WIDTH = 300, HEIGHT = 400;
    Gtk::Image icon;
    std::unique_ptr<WayfireMenuButton> button;
    Gtk::ScrolledWindow scrolled_window;
    Gtk::VBox vbox;

    Gtk::Label status_label = Gtk::Label("Cannot start notifications daemon. Probably another one is already running.");

    std::map<Notification::id_type, std::unique_ptr<WfSingleNotification>> notification_widgets = {};

    void newNotification(Notification::id_type id);
    void replaceNotification(Notification::id_type id);
    void closeNotification(Notification::id_type id);
    void onDaemonStop();
    void updateIcon();

    sigc::connection popover_timeout;
    WfOption<double> timeout{"panel/notifications_autohide_timeout"};
    WfOption<bool> show_critical_in_dnd{"panel/notifications_critical_in_dnd"};
    bool dnd_enabled = false;

    public:
    void init(Gtk::HBox *container) override;
    ~WayfireNotificationCenter() override = default;
};

#endif
