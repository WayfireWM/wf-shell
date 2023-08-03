#ifndef WIDGETS_MENU_HPP
#define WIDGETS_MENU_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <giomm/desktopappinfo.h>
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/window.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/separator.h>
#include <set>

class WayfireMenu;
using AppInfo = Glib::RefPtr<Gio::AppInfo>;

class WfMenuMenuItem : public Gtk::HBox
{
  public:
    WfMenuMenuItem(WayfireMenu *menu, AppInfo app);

    bool matches(Glib::ustring text);
    bool fuzzy_match(Glib::ustring text);
    bool operator <(const WfMenuMenuItem& other);

  private:
    WayfireMenu *menu;
    Gtk::Box m_left_pad, m_right_pad;
    Gtk::Button m_button;
    Gtk::VBox m_button_box;
    Gtk::Image m_image;
    Gtk::Label m_label;

    AppInfo m_app_info;
    void on_click();
};

class WayfireLogoutUIButton
{
  public:
    Gtk::VBox layout;
    Gtk::Image image;
    Gtk::Label label;
    Gtk::Button button;
};

class WayfireLogoutUI
{
  public:
    WayfireLogoutUI();
    WfOption<std::string> logout_command{"panel/logout_command"};
    WfOption<std::string> reboot_command{"panel/reboot_command"};
    WfOption<std::string> shutdown_command{"panel/shutdown_command"};
    WfOption<std::string> suspend_command{"panel/suspend_command"};
    WfOption<std::string> hibernate_command{"panel/hibernate_command"};
    WfOption<std::string> switchuser_command{"panel/switchuser_command"};
    Gtk::Window ui, bg;
    Gtk::HBox bg_box;
    WayfireLogoutUIButton logout;
    WayfireLogoutUIButton reboot;
    WayfireLogoutUIButton shutdown;
    WayfireLogoutUIButton suspend;
    WayfireLogoutUIButton hibernate;
    WayfireLogoutUIButton switchuser;
    WayfireLogoutUIButton cancel;
    Gtk::VBox main_layout, vspacing_layout;
    Gtk::HBox top_layout, middle_layout, bottom_layout, hspacing_layout;
    void create_logout_ui_button(WayfireLogoutUIButton *button,
        const char *icon, const char *label);
    void on_logout_click();
    void on_reboot_click();
    void on_shutdown_click();
    void on_suspend_click();
    void on_hibernate_click();
    void on_switchuser_click();
    void on_cancel_click();
};

class WayfireMenu : public WayfireWidget
{
    Gtk::Box flowbox_container;
    Gtk::HBox hbox, hbox_bottom;
    Gtk::VBox bottom_pad;
    Gtk::VBox popover_layout_box;
    Gtk::Separator separator;
    Gtk::Image main_image;
    Gtk::Entry search_box;
    Gtk::FlowBox flowbox;
    Gtk::Button logout_button;
    Gtk::ScrolledWindow scrolled_window;
    std::unique_ptr<WayfireMenuButton> button;
    std::unique_ptr<WayfireLogoutUI> logout_ui;

    GAppInfoMonitor *app_info_monitor = g_app_info_monitor_get();
    guint app_info_monitor_changed_handler_id;

    void load_menu_item(AppInfo app_info);
    void load_menu_items_from_dir(std::string directory);
    void load_menu_items_all();

    bool update_icon();

    bool fuzzy_filter     = false;
    int32_t count_matches = 0;

    bool on_sort(Gtk::FlowBoxChild*, Gtk::FlowBoxChild*);
    bool on_filter(Gtk::FlowBoxChild *child);
    void on_search_changed();
    void on_popover_shown();

    std::vector<std::unique_ptr<WfMenuMenuItem>> items;
    /* loaded_apps is a list of the already-opened applications + their execs,
     * so that we don't show duplicate entries */
    std::set<std::pair<std::string, std::string>> loaded_apps;

    WfOption<std::string> menu_logout_command{"panel/menu_logout_command"};
    WfOption<bool> fuzzy_search_enabled{"panel/menu_fuzzy_search"};
    WfOption<std::string> panel_position{"panel/position"};
    WfOption<std::string> menu_icon{"panel/menu_icon"};
    WfOption<int> menu_size{"panel/launchers_size"};
    WfOption<int> menu_min_content_width{"panel/menu_min_content_width"};
    WfOption<int> menu_min_content_height{"panel/menu_min_content_height"};
    void update_popover_layout();
    void create_logout_ui();
    void on_logout_click();

  public:
    void init(Gtk::HBox *container) override;
    void hide_menu();
    void refresh();
    ~WayfireMenu() override
    {
        g_signal_handler_disconnect(app_info_monitor, app_info_monitor_changed_handler_id);
    }
};

#endif /* end of include guard: WIDGETS_MENU_HPP */
