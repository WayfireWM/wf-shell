#ifndef LAUNCHERS_HPP
#define LAUNCHERS_HPP

#include "../widget.hpp"
#include <vector>
#include <giomm/desktopappinfo.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/image.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/eventbox.h>
#include <wayfire/util/duration.hpp>

#define LAUNCHERS_ICON_SCALE 1.42

struct LauncherInfo
{
    virtual std::string get_text() = 0;
    virtual std::string get_icon() = 0;
    virtual void execute() = 0;
    virtual ~LauncherInfo()
    {}
};

class LauncherAnimation :
    public wf::animation::duration_t,
    public wf::animation::timed_transition_t
{
  public:
    LauncherAnimation(wf::option_sptr_t<int> length, int start, int end) :
        duration_t(length, wf::animation::smoothing::linear),
        timed_transition_t((duration_t&)*this)
    {
        this->set(start, end);
        this->duration_t::start();
    }
};

struct WfLauncherButton
{
    std::string launcher_name;
    int32_t base_size;

    Glib::RefPtr<Gdk::Pixbuf> image;
    Gtk::EventBox evbox;
    LauncherInfo *info = NULL;
    LauncherAnimation current_size{wf::create_option(1000), 0, 0};
    WfOption<int> size{"panel/launchers_size"};

    WfLauncherButton();
    WfLauncherButton(const WfLauncherButton& other) = delete;
    WfLauncherButton& operator =(const WfLauncherButton&) = delete;
    ~WfLauncherButton();

    bool initialize(std::string name, std::string icon = "none", std::string label = "");

    Glib::RefPtr<Gdk::Pixbuf> get_pixbuf(std::string icon, int32_t size);
    bool on_click(GdkEventButton *ev);
    bool on_enter(GdkEventCrossing *ev);
    bool on_leave(GdkEventCrossing *ev);

    void update_size();
};

using launcher_container = std::vector<std::unique_ptr<WfLauncherButton>>;
class WayfireLaunchers : public WayfireWidget
{
    Gtk::HBox box;
    launcher_container launchers;
    launcher_container get_launchers_from_config();

  public:
    virtual void init(Gtk::HBox *container);
    virtual void handle_config_reload();
    virtual ~WayfireLaunchers()
    {}
};


#endif /* end of include guard: LAUNCHERS_HPP */
