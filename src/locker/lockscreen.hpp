#pragma once
#include <gtkmm.h>
#include <giomm.h>
#include <memory>
#include <sigc++/connection.h>

#include "background-gl.hpp"
#include "gtkmm/overlay.h"
#include "wf-option-wrap.hpp"
#include "lockergrid.hpp"
class WayfireLockerAppLockscreen: public Gtk::Window
{
    public:
    Gtk::Overlay overlay;
    BackgroundGLArea background;
    std::shared_ptr<WayfireLockerGrid> grid;
    sigc::connection timeout;
    WfOption<int> hide_timeout {"locker/hide_time"};
    WfOption<bool> wf_background {"locker/background_image"};
    std::vector<sigc::connection> signals;
    int last_x=-1, last_y=-1;

    WayfireLockerAppLockscreen(std::string background_path);

    void start_disappear_timer();
    void disconnect();

    void window_activity();
};