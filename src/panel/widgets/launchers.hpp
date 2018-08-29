#ifndef LAUNCHERS_HPP
#define LAUNCHERS_HPP

#include "../widget.hpp"
#include <vector>
#include <giomm/desktopappinfo.h>
#include <gtkmm/image.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/button.h>

struct LauncherInfo
{
    virtual Gtk::Image& get_image() = 0;
    virtual std::string get_text() = 0;
    virtual void execute() = 0;
    virtual ~LauncherInfo() {}
};

struct WfLauncherButton
{
    std::string launcher_name;
    Gtk::Button button;
    LauncherInfo *info = NULL;

    WfLauncherButton() {}
    WfLauncherButton(const WfLauncherButton& other) = delete;
    WfLauncherButton(const WfLauncherButton&& other) = delete;
    WfLauncherButton& operator = (const WfLauncherButton&) = delete;
    WfLauncherButton& operator = (const WfLauncherButton&&) = delete;

    ~WfLauncherButton();

    bool initialize(std::string name, std::string icon = "none");
    void on_click();
};

using launcher_container = std::vector<WfLauncherButton*>;
class WayfireLaunchers : public WayfireWidget
{
    Gtk::HBox box;
    launcher_container launchers;
    launcher_container get_launchers_from_config(wayfire_config *config);

    public:
        virtual void init(Gtk::HBox *container, wayfire_config *config);
        virtual ~WayfireLaunchers() {};
};


#endif /* end of include guard: LAUNCHERS_HPP */
