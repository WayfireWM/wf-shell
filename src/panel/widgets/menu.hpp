#ifndef WIDGETS_MENU_HPP
#define WIDGETS_MENU_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <giomm/desktopappinfo.h>
#include <gtkmm.h>
#include <set>

class WayfireMenu;
using AppInfo = Glib::RefPtr<Gio::DesktopAppInfo>;

class WfMenuCategory
{
  public:
    WfMenuCategory(std::string name, std::string icon_name);
    std::string get_name();
    std::string get_icon_name();
    std::vector<AppInfo> items;

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
    Gtk::Box m_box;
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
    WfMenuMenuItem(WayfireMenu *menu, AppInfo app);

    uint32_t matches(Glib::ustring text);
    uint32_t fuzzy_match(Glib::ustring text);
    bool operator <(const WfMenuMenuItem& other);
    void set_search_value(uint32_t value);
    uint32_t get_search_value();
    void on_click();

  private:
    WayfireMenu *menu;
    Gtk::Box m_left_pad, m_right_pad;
    Gtk::Box m_padding_box;
    Gtk::Box m_button_box;
    Gtk::Button m_button;
    Gtk::Box m_list_box;
    Gtk::Image m_image;
    Gtk::Label m_label;
    Glib::RefPtr<Gio::Menu> m_menu;
    Glib::RefPtr<Gio::SimpleActionGroup> m_actions;
    Gtk::MenuButton m_extra_actions_button;

    bool m_has_actions = false;
    uint32_t m_search_value = 0;

    AppInfo m_app_info;
};

class WayfireLogoutUIButton
{
  public:
    Gtk::Box layout;
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
    Gtk::Window ui;
    WayfireLogoutUIButton logout;
    WayfireLogoutUIButton reboot;
    WayfireLogoutUIButton shutdown;
    WayfireLogoutUIButton suspend;
    WayfireLogoutUIButton hibernate;
    WayfireLogoutUIButton switchuser;
    WayfireLogoutUIButton cancel;
    Gtk::CenterBox hbox;
    Gtk::Grid main_layout;
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
    WayfireOutput *output;

    std::string search_contents = "";

    Gtk::Box flowbox_container;
    Gtk::Box hbox, hbox_bottom, scroll_pair;
    Gtk::Box bottom_pad;
    Gtk::Box popover_layout_box;
    Gtk::Box category_box;
    Gtk::Separator separator;
    Gtk::Image main_image;
    Gtk::SearchEntry search_entry;
    Gtk::FlowBox flowbox;
    Gtk::Button logout_button;
    Gtk::Image logout_image;
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

    bool m_sort_names     = true;
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
    WfOption<int> menu_min_category_width{"panel/menu_min_category_width"};
    WfOption<int> menu_min_content_height{"panel/menu_min_content_height"};
    WfOption<bool> menu_show_categories{"panel/menu_show_categories"};
    void update_popover_layout();
    void update_category_width();
    void update_content_height();
    void update_content_width();
    void create_logout_ui();
    void on_logout_click();
    void key_press_search();
    void select_first_flowbox_item();

  public:
    void arrow_key(Gtk::DirectionType dir);
    void init(Gtk::Box *container) override;
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
