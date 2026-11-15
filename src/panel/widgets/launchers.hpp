#pragma once

#include "../widget.hpp"
#include <vector>
#include <giomm/desktopappinfo.h>
#include <gtkmm/image.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/button.h>
#include <wayfire/util/duration.hpp>
#include <glibmm/extraclassinit.h>

class WayfireLauncherButton : public Glib::ExtraClassInit, public Gtk::Image
{
  public:
    WayfireLauncherButton() :
        Glib::ObjectBase("launcher"),
        Glib::ExtraClassInit(&WayfireLauncherButton::class_init_func),
        Gtk::Image()
    {}

  private:
    static void class_init_func(void *klass, void* /*data*/)
    {
        auto *widget_class = GTK_WIDGET_CLASS(klass);
        gtk_widget_class_set_accessible_role(widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
        gtk_widget_class_set_css_name(widget_class, "launcher");
    }
};


struct WfLauncherButton
{
    WayfireLauncherButton m_icon;
    sigc::connection btn_sig;
    Glib::RefPtr<Gio::DesktopAppInfo> app_info;

    WfLauncherButton();
    WfLauncherButton(const WfLauncherButton& other) = delete;
    WfLauncherButton& operator =(const WfLauncherButton&) = delete;
    ~WfLauncherButton();

    bool initialize(std::string name, std::string icon = "none", std::string label = "");
    void launch();
    void update_icon();
};

using launcher_container = std::vector<std::unique_ptr<WfLauncherButton>>;
class WayfireLaunchers : public WayfireWidget
{
    Gtk::FlowBox box;
    launcher_container launchers;
    launcher_container get_launchers_from_config();

    WfOption<int> spacing{"panel/launchers_spacing"};
    WfOption<int> rows_cols{"panel/launchers_rows_cols"};

  public:
    virtual void init(Gtk::Box *container);
    void update_layout();
    virtual void handle_config_reload();
    virtual ~WayfireLaunchers()
    {}
};
