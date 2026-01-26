#include <iostream>
#include <giomm/application.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/box.h>
#include <gtkmm/centerbox.h>
#include <gtkmm/application.h>
#include <gdkmm/monitor.h>
#include <gdk/wayland/gdkwayland.h>
#include <gtk4-session-lock.h>
#include <memory>
#include <wayfire/config/file.hpp>

#include "css-config.hpp"
#include "lockergrid.hpp"
#include "lockscreen.hpp"
#include "plugin/battery.hpp"
#include "plugin/clock.hpp"
#include "plugin/weather.hpp"
#include "plugin/instant.hpp"
#include "plugin/password.hpp"
#include "plugin/pin.hpp"
#include "plugin/fingerprint.hpp"
#include "plugin/user.hpp"
#include "plugin/volume.hpp"
#include "plugin/mpris.hpp"
#include "wf-option-wrap.hpp"
#include "wf-shell-app.hpp"
#include "locker.hpp"

Gio::Application::Flags WayfireLockerApp::get_extra_application_flags()
{
    return Gio::Application::Flags::NONE;
}

std::string WayfireLockerApp::get_application_name()
{
    return "org.wayfire.locker";
}

WayfireLockerApp::WayfireLockerApp() 
{}

WayfireLockerApp::~WayfireLockerApp()
{}

void WayfireLockerApp::perform_lock()
{

    if (is_debug())
    {
        if(!m_is_locked)
        {
            on_monitor_present(nullptr);
        }
    } else
    {
        /* Demand the session be locked */
        gtk_session_lock_instance_lock(lock);
    }
}

/* Called before each activate */
void WayfireLockerApp::command_line()
{
    //virtual bool parse_cfgfile(const Glib::ustring & option_name,
    ///    const Glib::ustring & value, bool has_value)
    can_early_wake = true;
    app->add_main_option_entry(
        [=](const Glib::ustring & option_name,
                 const Glib::ustring & value, bool has_value){
            can_early_wake = false;
            return true;
        },
        "now", 'n', "Instant lock", "", Glib::OptionEntry::Flags::NO_ARG);
}

void WayfireLockerApp::on_activate()
{
    if (activated)
    {
        if (!m_is_locked)
        {
            if (can_early_wake)
            {
                Glib::signal_timeout().connect_seconds([this](){
                    can_early_wake = false;
                    return G_SOURCE_REMOVE;
                }, WfOption<int> {"locker/prewake"});
            }
            perform_lock();
        }
        return;
    }
    /* Set a timer for early-wake unlock */
    WayfireShellApp::on_activate();
        Glib::signal_timeout().connect_seconds([this](){
        can_early_wake = false;
        return G_SOURCE_REMOVE;
    }, WfOption<int> {"locker/prewake"});
    /* TODO Hot config for this? */
    exit_on_unlock = WfOption<bool>{"locker/exit_on_unlock"};

    auto debug = Glib::getenv("WF_LOCKER_DEBUG");
    if (debug == "1")
    {
        m_is_debug = true;
    }

    lock = gtk_session_lock_instance_new();
    /* Session lock callbacks */
    g_signal_connect(lock, "locked", G_CALLBACK(on_session_locked_c), lock);
    g_signal_connect(lock, "failed", G_CALLBACK(on_session_lock_failed_c), lock);
    g_signal_connect(lock, "unlocked", G_CALLBACK(on_session_unlocked_c), lock);
    g_signal_connect(lock, "monitor", G_CALLBACK(on_monitor_present_c), lock);
    alternative_monitors = true; /* Don't use WayfireShellApp monitor tracking, we get a different set */
    new CssFromConfigString("locker/background_color", ".wf-locker {background-color:", ";}");
    new CssFromConfigFont("locker/clock_font", ".wf-locker .clock {", "}");
    new CssFromConfigFont("locker/weather_font", ".wf-locker .weather {", "}");
    new CssFromConfigFont("locker/user_font", ".wf-locker .user {", "}");
    new CssFromConfigFont("locker/pin_pad_font", ".wf-locker .pinpad-button {", "}");
    new CssFromConfigFont("locker/pin_reply_font", ".wf-locker .pinpad-current {", "}");
    new CssFromConfigFont("locker/fingerprint_font", ".wf-locker .fingerprint-text {", "}");
    new CssFromConfigFont("locker/battery_percent_font", ".wf-locker .battery-percent {", "}");
    new CssFromConfigFont("locker/battery_description_font", ".wf-locker .battery-description {", "}");
    new CssFromConfigFont("locker/instant_unlock_font", ".wf-locker .instant-unlock {", "}");
    new CssFromConfigInt("locker/battery_icon_size", ".wf-locker .battery-image {-gtk-icon-size:", "px;}");
    new CssFromConfigInt("locker/fingerprint_icon_size", ".wf-locker .fingerprint-icon {-gtk-icon-size:",
        "px;}");
    new CssFromConfigInt("locker/prewake", ".fade-in {animation-name: slowfade;animation-duration: ", "s; animation-timing-function: linear; animation-iteration-count: 1; animation-fill-mode: forwards;} @keyframes slowfade { from {opacity:0;} to {opacity:1;}}");

    /* Init plugins */
    plugins.emplace("clock", Plugin(new WayfireLockerClockPlugin()));
    plugins.emplace("weather", Plugin(new WayfireLockerWeatherPlugin()));
    plugins.emplace("battery", Plugin(new WayfireLockerBatteryPlugin()));
    plugins.emplace("password", Plugin(new WayfireLockerPasswordPlugin()));
    plugins.emplace("instant", (Plugin(new WayfireLockerInstantPlugin())));
    plugins.emplace("pin", Plugin(new WayfireLockerPinPlugin()));
    plugins.emplace("fingerprint", Plugin(new WayfireLockerFingerprintPlugin()));
    plugins.emplace("volume", Plugin(new WayfireLockerVolumePlugin()));
    plugins.emplace("mpris", Plugin(new WayfireLockerMPRISPlugin()));
    plugins.emplace("aboutuser", Plugin(new WayfireLockerUserPlugin()));

    /* Create plugin option callbacks */
    for (auto &it : plugins)
    {
        Plugin plugin = it.second;
        plugin->enable.set_callback(
            [plugin, this] () {
                if (plugin->enable)
                {
                    plugin->init();
                    for(auto &it : window_list)
                    {
                        int id = it.first;
                        plugin->add_output(id, it.second->grid);
                    }
                } else {
                    for(auto &it : window_list)
                    {
                        int id = it.first;
                        plugin->remove_output(id, it.second->grid);
                    }
                    plugin->deinit();
                }
            }
        );
        plugin->position.set_callback(
            [this, plugin] () {
                for(auto &it : window_list)
                {
                    int id = it.first;
                    auto window = it.second;
                    plugin->remove_output(id, window->grid);
                    plugin->add_output(id, window->grid);
                }
            }
        );
    }
    perform_lock();
}

/** Called just as lock starts but before window is shown */
void WayfireLockerApp::init_plugins()
{
    for (auto &it : plugins)
    {
        Plugin plugin = it.second;
        if (plugin->enable)
        {
            it.second->init();
        }
    }
}

/** Called after an unlock */
void WayfireLockerApp::deinit_plugins()
{
    for (auto &it : plugins)
    {
        Plugin plugin = it.second;
        if (plugin->enable)
        {
            it.second->deinit();
        }
    }
}

/* A new monitor has been added to the lockscreen */
void WayfireLockerApp::on_monitor_present(GdkMonitor *monitor)
{
    int id = window_id_count;
    window_id_count++;
    /* Create lockscreen with a grid for contents */
    window_list.emplace(id, new WayfireLockerAppLockscreen());
    auto window = window_list[id];

    for (auto& it : plugins)
    {
        Plugin plugin = it.second;
        if (plugin->enable)
        {
            it.second->add_output(id, window->grid);
        }
    }

    window->signal_close_request().connect([this, id] ()
    {
        for (auto& it : plugins)
        {
            Plugin plugin = it.second;
            if (plugin->enable)
            {
                plugin->remove_output(id, window_list[id]->grid);
            }
        }
        if (m_is_debug)
        {
            deinit_plugins();
            m_is_locked = false;
            if (exit_on_unlock)
            {
                exit(0);
            }
        }

        return false;
    }, false);
    if (is_debug())
    {
        init_plugins();
        m_is_locked = true;
        window->present();
    } else
    {
        gtk_session_lock_instance_assign_window_to_monitor(lock, window->gobj(), monitor);
    }
}

/* Called on any successful auth to unlock & end locker */
void WayfireLockerApp::perform_unlock(std::string reason)
{
    /* Offset the actual logic so that any callbacks that call
       this get a chance to exit cleanly. */
    Glib::signal_idle().connect([this, reason] () {
        std::cout << "Unlocked : " << reason << std::endl;
        if (m_is_debug)
        {
            /* We need to manually close in debug mode */
            for(auto &it : window_list)
            {
                it.second->close();
            }
            if (WayfireLockerApp::get().exit_on_unlock)
            {
                exit(0);
            }
        } else
        {
            gtk_session_lock_instance_unlock(lock);
        }
        for(auto &it : window_list)
        {
            it.second->disconnect();
        }
        return G_SOURCE_REMOVE;
    });
}

void WayfireLockerApp::create(int argc, char **argv)
{
    if (instance)
    {
        throw std::logic_error("Running WayfireLockerApp twice!");
    }

    instance = std::unique_ptr<WayfireShellApp>(new WayfireLockerApp{});
    instance->init_app();
    instance->run(argc, argv);
}

bool WayfireLockerApp::is_locked()
{
    if (!m_is_debug)
    {
        return gtk_session_lock_instance_is_locked(lock);
    }
    return m_is_locked;
}

void WayfireLockerApp::set_is_locked(bool locked)
{
    m_is_locked = locked;
}

/* Starting point */
int main(int argc, char **argv)
{
    if (!gtk_session_lock_is_supported())
    {
        std::cerr << "This session does not support locking" << std::endl;
        exit(0);
    }

    WayfireLockerApp::create(argc, argv);
    return 0;
}

/* lock session calllbacks */
void on_session_locked_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session locked" << std::endl;
    WayfireLockerApp::get().set_is_locked(true);
    WayfireLockerApp::get().init_plugins();
}

void on_session_lock_failed_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session lock failed" << std::endl;
    WayfireLockerApp::get().set_is_locked(false);
    if (WayfireLockerApp::get().exit_on_unlock)
    {
        exit(0);
    }
}

void on_session_unlocked_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session unlocked" << std::endl;
    WayfireLockerApp::get().set_is_locked(false);
    WayfireLockerApp::get().deinit_plugins();
    if (WayfireLockerApp::get().exit_on_unlock)
    {
        // Exiting too early causes a lock-out
        Glib::signal_timeout().connect_seconds([] () -> bool 
        {
            exit(0);
        },1);
    }
    // Lose windows
    WayfireLockerApp::get().window_list.clear();
    // Replace the lock object
    //lock = gtk_session_lock_instance_new();
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
    if (plugins.find(name) == plugins.end())
    {
        return nullptr;
    }

    return plugins.at(name);
}

void WayfireLockerApp::user_activity()
{
    if (can_early_wake)
    {
        can_early_wake = false;
        perform_unlock("Early Activity");
    }
}