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

class WfMenuCategory
{
  public:
    WfMenuCategory(std::string name, std::string icon_name);
    std::string get_name();
    std::string get_icon_name();
    std::vector<Glib::RefPtr<Gio::DesktopAppInfo>> items;

  private:
    std::string name;
    std::string icon_name;
};

class WfMenuCategoryButton : public Gtk::Button
{
  public:
    WfMenuCategoryButton(WayfireMenu *menu, std::string category, std::string label, std::string icon_name);

  private:
    WayfireMenu *menu;
    Gtk::HBox m_box;
    Gtk::Label m_label;
    Gtk::Image m_image;

    std::string category;
    std::string label;
    std::string icon_name;
    void on_click();
};

class WfMenuMenuItem : public Gtk::FlowBoxChild
{
  public:
    WfMenuMenuItem(WayfireMenu *menu, Glib::RefPtr<Gio::DesktopAppInfo> app);

    bool matches(Glib::ustring text);
    bool fuzzy_match(Glib::ustring text);
    bool operator <(const WfMenuMenuItem& other);

  private:
    WayfireMenu *menu;
    Gtk::Box m_left_pad, m_right_pad;
    Gtk::HBox m_padding_box;
    Gtk::VBox m_button_box;
    Gtk::HBox m_list_box;
    Gtk::Image m_image;
    Gtk::Label m_label;
    Gtk::Menu m_action_menu;

    bool m_has_actions = false;

    Glib::RefPtr<Gio::DesktopAppInfo> m_app_info;
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

class WayfireMenuInjectionEntry : public Gtk::Entry
{
  public:
    bool inject(GdkEventKey *ev);
};

class WayfireMenu : public WayfireWidget
{
    WayfireOutput *output;

    Gtk::Box flowbox_container;
    Gtk::HBox hbox, hbox_bottom, scroll_pair;
    Gtk::VBox bottom_pad;
    Gtk::VBox popover_layout_box;
    Gtk::VBox category_box;
    Gtk::Separator separator;
    Gtk::Image main_image;
    WayfireMenuInjectionEntry search_box;
    Gtk::FlowBox flowbox;
    Gtk::Button logout_button;
    Gtk::ScrolledWindow app_scrolled_window, category_scrolled_window;
    std::unique_ptr<WayfireMenuButton> button;
    std::unique_ptr<WayfireLogoutUI> logout_ui;

    GAppInfoMonitor *app_info_monitor = g_app_info_monitor_get();
    guint app_info_monitor_changed_handler_id;

    void load_menu_item(AppInfo app_info);
    void load_menu_items_from_dir(std::string directory);
    void load_menu_items_all();

    void add_category_app(std::string category, Glib::RefPtr<Gio::DesktopAppInfo>);

    bool update_icon();

    bool fuzzy_filter     = false;
    int32_t count_matches = 0;

    bool on_sort(Gtk::FlowBoxChild*, Gtk::FlowBoxChild*);
    bool on_filter(Gtk::FlowBoxChild *child);
    void on_search_changed();
    void on_popover_shown();

    /* loaded_apps is a list of the already-opened applications + their execs,
     * so that we don't show duplicate entries */
    std::set<std::pair<std::string, std::string>> loaded_apps;
    std::unordered_map<std::string, std::unique_ptr<WfMenuCategory>> category_list;
    std::string category = "All";
    std::vector<std::string> category_order = {
        "All", "Network", "Education", "Office", "Development", "Graphics", "AudioVideo", "Game", "Science",
        "Settings", "System", "Utility", "Hidden"
    };

    WfOption<std::string> menu_logout_command{"panel/menu_logout_command"};
    WfOption<bool> fuzzy_search_enabled{"panel/menu_fuzzy_search"};
    WfOption<std::string> panel_position{"panel/position"};
    WfOption<std::string> menu_icon{"panel/menu_icon"};
    WfOption<int> menu_size{"panel/launchers_size"};
    WfOption<int> menu_min_category_width{"panel/menu_min_category_width"};
    WfOption<int> menu_min_content_height{"panel/menu_min_content_height"};
    WfOption<bool> menu_show_categories{"panel/menu_show_categories"};
    void update_popover_layout();
    void create_logout_ui();
    void on_logout_click();
    void key_press_search();
    void select_first_flowbox_item();
    void set_default_to_selection();

  public:
    void init(Gtk::HBox *container) override;
    void populate_menu_items(std::string category);
    void populate_menu_categories();
    void toggle_menu();
    void hide_menu();
    void refresh();
    void set_category(std::string category);
    WfOption<bool> menu_list{"panel/menu_list"};
    WfOption<int> menu_min_content_width{"panel/menu_min_content_width"};

    WayfireMenu(WayfireOutput *output)
    {
        this->output = output;
    }

    ~WayfireMenu() override
    {
        g_signal_handler_disconnect(app_info_monitor, app_info_monitor_changed_handler_id);
    }
};

#endif /* end of include guard: WIDGETS_MENU_HPP */
