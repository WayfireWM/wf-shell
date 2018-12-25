#ifndef WIDGETS_MENU_HPP
#define WIDGETS_MENU_HPP

#include "../widget.hpp"
#include <giomm/desktopappinfo.h>
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/popover.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/scrolledwindow.h>
#include <set>

class WfMenuMenuItem : public Gtk::Button
{
    public:
    using AppInfo = Glib::RefPtr<Gio::DesktopAppInfo>;
    WfMenuMenuItem(AppInfo app);

    bool matches(Glib::ustring text);
    bool operator < (const WfMenuMenuItem& other);

    private:
    Gtk::VBox m_button_box;
    Gtk::Image m_image;
    Gtk::Label m_label;

    AppInfo m_app_info;
    void on_click();
};

class WayfireMenu : public WayfireWidget
{
    Gtk::Box flowbox_container;
    Gtk::HBox hbox;
    Gtk::VBox bottom_pad;
    Gtk::VBox box;
    Gtk::Image main_image;
    Gtk::Entry search_box;
    Gtk::FlowBox flowbox;
    Gtk::Popover popover;
    Gtk::MenuButton menu_button;
    Gtk::ScrolledWindow scrolled_window;

    void load_menu_item(std::string file);
    void load_menu_items(std::string directory);

    void focus_lost() override;

    bool on_sort(Gtk::FlowBoxChild*, Gtk::FlowBoxChild*);
    bool on_filter(Gtk::FlowBoxChild* child);
    void on_search_changed();
    void on_popover_shown();

    std::vector<std::unique_ptr<WfMenuMenuItem>> items;
    /* loaded_apps is a list of the already-opened applications + their execs,
     * so that we don't show duplicate entries */
    std::set<std::pair<std::string, std::string>> loaded_apps;

    public:
    void init(Gtk::HBox *container, wayfire_config *config) override;
};

#endif /* end of include guard: WIDGETS_MENU_HPP */
