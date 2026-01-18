#include <glibmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/application.h>
#include <gdk/wayland/gdkwayland.h>

#include <iostream>

#include <css-config.hpp>
#include "locker.hpp"

#include "gdkmm/monitor.h"
#include "glibmm/miscutils.h"
#include "gtk4-session-lock.h"
#include "gtkmm/enums.h"

#include "plugin/battery.hpp"
#include "plugin/clock.hpp"
#include "plugin/instant.hpp"
#include "plugin/password.hpp"
#include "plugin/pin.hpp"
#include "plugin/fingerprint.hpp"
#include "wf-shell-app.hpp"
#include <wayfire/config/file.hpp>

WayfireLockerApp::~WayfireLockerApp(){

}

void WayfireLockerApp::on_activate()
{
    WayfireShellApp::on_activate();
    auto debug = Glib::getenv("WF_LOCKER_DEBUG");
    if(debug == "1"){
        m_is_debug = true;
    }
    std::cout << "Locker activate" << std::endl;
    lock = gtk_session_lock_instance_new();
    /* Session lock callbacks */
    g_signal_connect(lock, "locked", G_CALLBACK(on_session_locked_c), lock);
    g_signal_connect(lock, "failed", G_CALLBACK(on_session_lock_failed_c), lock);
    g_signal_connect(lock, "unlocked", G_CALLBACK(on_session_unlocked_c), lock);
    g_signal_connect(lock, "monitor", G_CALLBACK(on_monitor_present_c), lock);
    alternative_monitors = true; /* Don't use WayfireShellApp monitor tracking, we get a different set */
    new CssFromConfigString("locker/background_color", ".wf-locker {background-color:", ";}");
    new CssFromConfigFont("locker/clock_font", ".wf-locker .clock {", "}");
    new CssFromConfigFont("locker/pin_pad_font", ".wf-locker .pinpad-button {", "}");
    new CssFromConfigFont("locker/pin_reply_font", ".wf-locker .pinpad-current {", "}");
    new CssFromConfigFont("locker/fingerprint_font", ".wf-locker .fingerprint-text {", "}");
    new CssFromConfigFont("locker/battery_percent_font", ".wf-locker .battery-percent {", "}");
    new CssFromConfigFont("locker/battery_description_font", ".wf-locker .battery-description {", "}");
    new CssFromConfigFont("locker/instant_unlock_font", ".wf-locker .instant-unlock {", "}");
    new CssFromConfigInt("locker/battery_icon_size", ".wf-locker .battery-image {-gtk-icon-size:", "px;}");
    new CssFromConfigInt("locker/fingerprint_icon_size", ".wf-locker .fingerprint-icon {-gtk-icon-size:", "px;}");

    /* Init plugins */
    plugins.emplace("clock", Plugin(new WayfireLockerClockPlugin()));
    plugins.emplace("battery", Plugin(new WayfireLockerBatteryPlugin()));
    plugins.emplace("password",Plugin(new WayfireLockerPasswordPlugin()));
    plugins.emplace("instant",(Plugin(new WayfireLockerInstantPlugin())));
    plugins.emplace("pin",Plugin(new WayfireLockerPinPlugin()));
    plugins.emplace("fingerprint", Plugin(new WayfireLockerFingerprintPlugin()));

    for(auto& it: plugins){
        if(it.second->should_enable())
        {
            it.second->init();
        }
    }

    if(is_debug())
    {
        on_monitor_present(nullptr);
    } else {
        /* Demand the session be locked */
        gtk_session_lock_instance_lock(lock);
    }
}

/* A new monitor has been added to the lockscreen */
void WayfireLockerApp::on_monitor_present(GdkMonitor* monitor)
{
    int id = window_id_count;
    window_id_count ++;
    /* Create lockscreen with a grid for contents */
    auto window = new Gtk::Window();
    window->add_css_class("wf-locker");
    auto grid = new Gtk::Grid();
    window->set_child(*grid);
    grid->set_expand(true);
    grid->set_column_homogeneous(true);
    grid->set_row_homogeneous(true);
    for(int x = 0; x < 3; x ++)
    {
        for(int y = 0; y < 3; y ++)
        {
            auto box = new Gtk::Box();
            if(x == 0)
            {
                box->set_halign(Gtk::Align::START);
            } else if (x == 2)
            {
                box->set_halign(Gtk::Align::END);
            }
            if(y == 0)
            {
                box->set_valign(Gtk::Align::START);
            } else if (y == 2)
            {
                box->set_valign(Gtk::Align::END);
            }
            box->set_orientation(Gtk::Orientation::VERTICAL);
            grid->attach(*box, x, y);
        }
    }
    for(auto& it: plugins){
        if(it.second->should_enable())
        {
            it.second->add_output(id, grid);
        }
    }
    window->signal_close_request().connect([this, id](){
        for(auto& it: plugins){
            it.second->remove_output(id);
        }
        return false;
    },false);
    if(is_debug())
    {
        window->present();
    } else {
        gtk_session_lock_instance_assign_window_to_monitor(lock, window->gobj(), monitor);
    }
}

/* Called on any successful auth to unlock & end locker */
void WayfireLockerApp::unlock()
{
    if (is_debug())
    {
        exit(0);
    }
    gtk_session_lock_instance_unlock(lock);
}

void WayfireLockerApp::create(int argc, char **argv)
{
    if (instance)
    {
        throw std::logic_error("Running WayfireLockerApp twice!");
    }

    instance = std::unique_ptr<WayfireShellApp>(new WayfireLockerApp{});
    instance->run(argc, argv);
}

/* Starting point */
int main(int argc, char **argv)
{
    if(!gtk_session_lock_is_supported())
    {
        std::cerr << "This session does not support locking" <<std::endl;
    }    
    WayfireLockerApp::create(argc, argv);
    std::cout << "Exit" << std::endl;
    return 0;
}

/* lock session calllbacks */
void on_session_locked_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session locked" << std::endl;
}

void on_session_lock_failed_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session lock failed" << std::endl;
    exit(0);
}

void on_session_unlocked_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session unlocked" << std::endl;
    // Exiting here causes wf to lock up
    Glib::signal_timeout().connect_seconds([]()->bool{
        exit(0);
        return 0;
    },1);
}

void on_monitor_present_c(GtkSessionLockInstance *lock, GdkMonitor *monitor, void *data)
{
    WayfireLockerApp::get().on_monitor_present(monitor);
}
/* Find user config */
std::string WayfireLockerApp::get_config_file()
{
    if (cmdline_config.has_value())
    {
        return cmdline_config.value();
    }

    std::string config_dir;

    char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home == NULL)
    {
        config_dir = std::string(getenv("HOME")) + "/.config";
    } else
    {
        config_dir = std::string(config_home);
    }
    
    return config_dir + "/wf-shell.ini";
}

Plugin WayfireLockerApp::get_plugin(std::string name)
{
    if(plugins.find(name )==plugins.end())
    {
        return nullptr;
    }
    return plugins.at(name);
}
