#pragma once

#include <giomm/desktopappinfo.h>
#include <gtkmm.h>
#include <sigc++/connection.h>
#include <set>

#include "widget.hpp"
#include "wf-popover.hpp"

#include "layout.hpp"
#include "logout.hpp"
#include "category.hpp"

using AppInfo = Glib::RefPtr<Gio::DesktopAppInfo>;

class WayfireMenu : public WayfireWidget
{
    WayfireOutput *output;

  public:
    Gtk::Box flowbox_container;
    Gtk::Box box, box_bottom;
    Gtk::Box popover_layout_box;
    Gtk::Box category_box;
    Gtk::Separator separator;
    Gtk::Image main_image;
    Gtk::SearchEntry search_entry;
    Gtk::FlowBox flowbox;
    Gtk::Button logout_button;
    Gtk::Image logout_image;
    Gtk::ScrolledWindow app_scrolled_window, category_scrolled_window;

  private:
    std::shared_ptr<WfMenuLayout> layout;
    std::unique_ptr<WayfireMenuWidget> button;
    std::unique_ptr<WayfireLogoutUI> logout_ui;

    GAppInfoMonitor *app_info_monitor = nullptr;
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

    std::vector<sigc::connection> signals;

    WfOption<std::string> menu_logout_command{"panel/menu_logout_command"};
    WfOption<bool> fuzzy_search_enabled{"panel/menu_fuzzy_search"};
    WfOption<std::string> panel_position{"panel/position"};
    WfOption<std::string> menu_icon{"panel/menu_icon"};
    WfOption<int> flowbox_spacing{"panel/menu_item_spacing"};
    WfOption<int> menu_min_category_width{"panel/menu_min_category_width"};
    WfOption<int> menu_min_content_height{"panel/menu_min_content_height"};
    WfOption<bool> menu_show_categories{"panel/menu_show_categories"};
    void setup_popover_layout();
    void update_popover_layout();
    void update_size();
    void create_logout_ui();
    void on_logout_click();
    void key_press_search();
    void select_first_flowbox_item();

  public:
    void arrow_key(Gtk::DirectionType dir);
    void init(Gtk::Box *container) override;
    void populate_menu_items();
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

    ~WayfireMenu() override;
};
