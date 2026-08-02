#pragma once
#include <giomm/desktopappinfo.h>
#include <giomm/menu.h>
#include <giomm/simpleactiongroup.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/flowboxchild.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

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
    WayfireMenu *menu;
    Gtk::Box box, list_item;
    Gtk::Image image;
    Gtk::Label label;
    Glib::RefPtr<Gio::Menu> m_menu;
    Glib::RefPtr<Gio::SimpleActionGroup> actions;
    Gtk::Button button;
    Gtk::MenuButton extra_actions_button;
    std::vector<sigc::connection> signals;

    bool has_actions = false;
    uint32_t search_value = 0;

    AppInfo app_info;
};
