#ifndef LAUNCHERS_HPP
#define LAUNCHERS_HPP

#include "../widget.hpp"
#include <vector>
#include <animation.hpp>
#include <giomm/desktopappinfo.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/image.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/eventbox.h>

#define LAUNCHERS_ICON_SCALE 1.42

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
    int32_t base_size;
    int32_t current_size;

    Gtk::Image image;
    Gtk::EventBox evbox;
    LauncherInfo *info = NULL;

    wf_duration hover_animation;
    bool animation_running = false;

    WfLauncherButton();
    WfLauncherButton(const WfLauncherButton& other) = delete;
    WfLauncherButton& operator = (const WfLauncherButton&) = delete;
    ~WfLauncherButton();

    bool initialize(wayfire_config *config, std::string name,
                    std::string icon = "none");

    bool on_click(GdkEventButton *ev);
    bool on_enter(GdkEventCrossing *ev);
    bool on_leave(GdkEventCrossing *ev);
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& ctx);
    void on_scale_update();

    void set_size(int size);
};

using launcher_container = std::vector<std::unique_ptr<WfLauncherButton>>;
class WayfireLaunchers : public WayfireWidget
{
    Gtk::HBox box;
    launcher_container launchers;
    launcher_container get_launchers_from_config(wayfire_config *config);

    public:
        virtual void init(Gtk::HBox *container, wayfire_config *config);
        virtual void handle_config_reload(wayfire_config *config);
        virtual ~WayfireLaunchers() {};
};


#endif /* end of include guard: LAUNCHERS_HPP */
