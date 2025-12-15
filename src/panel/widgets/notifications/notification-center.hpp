#ifndef NOTIFICATION_CENTER_HPP
#define NOTIFICATION_CENTER_HPP

#include "../../widget.hpp"
#include "daemon.hpp"
#include "single-notification.hpp"

#include <gtkmm/scrolledwindow.h>
#include <wf-popover.hpp>

class WayfireNotificationCenter : public WayfireWidget
{
  private:
    static const int WIDTH = 300, HEIGHT = 400;

    const std::shared_ptr<Daemon> daemon = Daemon::Launch();
    sigc::connection notification_new_conn;
    sigc::connection notification_replace_conn;
    sigc::connection notification_close_conn;

    Gtk::Image icon;
    std::unique_ptr<WayfireMenuButton> button;
    Gtk::ScrolledWindow scrolled_window;
    Gtk::VBox vbox;

    std::map<Notification::id_type, std::unique_ptr<WfSingleNotification>> notification_widgets = {};

    void newNotification(Notification::id_type id, bool show_popup = true);
    void replaceNotification(Notification::id_type id);
    void closeNotification(Notification::id_type id);
    void updateIcon();

    sigc::connection popover_timeout;
    WfOption<double> timeout{"panel/notifications_autohide_timeout"};
    WfOption<bool> show_critical_in_dnd{"panel/notifications_critical_in_dnd"};
    WfOption<int> icon_size{"panel/notifications_icon_size"};
    bool dnd_enabled = false;

  public:
    void init(Gtk::HBox *container) override;
    ~WayfireNotificationCenter() override
    {
        notification_new_conn.disconnect();
        notification_replace_conn.disconnect();
        notification_close_conn.disconnect();
    }
};

#endif
