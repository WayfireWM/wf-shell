#ifndef LAUNCHERS_HPP
#define LAUNCHERS_HPP

#include "../widget.hpp"
#include <vector>
#include <giomm/desktopappinfo.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/image.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/button.h>

struct LauncherInfo
{
    virtual Glib::RefPtr<Gdk::Pixbuf> get_pixbuf(int32_t size) = 0;
    virtual std::string get_text() = 0;
    virtual void execute() = 0;
    virtual ~LauncherInfo() {}
};

struct WfLauncherButton
{
    std::string launcher_name;
    int32_t size;

    Gtk::Image image;
    Gtk::Button button;
    LauncherInfo *info = NULL;

    WfLauncherButton() {}
    WfLauncherButton(const WfLauncherButton& other) = delete;
    WfLauncherButton(const WfLauncherButton&& other) = delete;
    WfLauncherButton& operator = (const WfLauncherButton&) = delete;
    WfLauncherButton& operator = (const WfLauncherButton&&) = delete;

    ~WfLauncherButton();

    bool initialize(wayfire_config *config, std::string name,
                    std::string icon = "none");
    void on_click();

    void on_scale_update();
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
