#pragma once
#include <gtkmm/overlay.h>
#include <giomm/desktopappinfo.h>
#include <giomm/menu.h>
#include <giomm/simpleactiongroup.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/flowboxchild.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include "wf-option-wrap.hpp"

class WayfireMenu;
using AppInfo = Glib::RefPtr<Gio::DesktopAppInfo>;


class WfMenuItem : public Gtk::FlowBoxChild
{
  public:
    WfMenuItem(WayfireMenu *menu, AppInfo app);
    ~WfMenuItem();

    uint32_t matches(Glib::ustring text);
    uint32_t fuzzy_match(Glib::ustring text);
    bool operator <(const WfMenuItem& other);
    void set_search_value(uint32_t value);
    uint32_t get_search_value();
    void on_click();
    AppInfo get_app_info() const
    {
        return app_info;
    }

  private:
    WfOption<bool> menu_list{"panel/menu_list"};
    WfOption<std::string> panel_position{"panel/position"};


    WayfireMenu *menu;
    Gtk::Box box;
    Gtk::Image image;
    Gtk::Label label;
    Glib::RefPtr<Gio::Menu> m_menu;
    Glib::RefPtr<Gio::SimpleActionGroup> actions;
    Gtk::MenuButton extra_actions_button;
    Gtk::Overlay overlay;
    std::vector<sigc::connection> signals;

    bool has_actions = false;
    uint32_t search_value = 0;

    AppInfo app_info;
};
