#ifndef LAUNCHERS_HPP
#define LAUNCHERS_HPP

#include "../widget.hpp"
#include <vector>
#include <giomm/desktopappinfo.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <wayfire/util/duration.hpp>

struct WfLauncherButton
{
    Gtk::Image m_icon;
    Gtk::Button button;
    Glib::RefPtr<Gio::DesktopAppInfo> app_info;
    WfOption<int> icon_size{"panel/launchers_size"};

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
    Gtk::Box box;
    launcher_container launchers;
    launcher_container get_launchers_from_config();

  public:
    virtual void init(Gtk::Box *container);
    virtual void handle_config_reload();
    virtual ~WayfireLaunchers()
    {}
};


#endif /* end of include guard: LAUNCHERS_HPP */
