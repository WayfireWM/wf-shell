#pragma once
#include <gtkmm.h>
#include <giomm.h>
#include <memory>
#include <sigc++/connection.h>

#include "wf-option-wrap.hpp"
#include "lockergrid.hpp"
class WayfireLockerAppLockscreen: public Gtk::Window
{
    public:
    std::shared_ptr<WayfireLockerGrid> grid;
    Gtk::Revealer revealer;
    sigc::connection timeout;
    WfOption<int> hide_timeout {"locker/hide_time"};
    std::vector<sigc::connection> signals;
    int last_x=-1, last_y=-1;

    WayfireLockerAppLockscreen();

    void start_disappear_timer();
    void disconnect();
};