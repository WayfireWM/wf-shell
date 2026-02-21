#include <csignal>
#include <iostream>
#include <fstream>
#include <giomm/application.h>
#include <glibmm.h>
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
#include <sys/inotify.h>

#include "background-gl.hpp"
#include "css-config.hpp"
#include "glib.h"
#include "glibmm/main.h"
#include "lockscreen.hpp"
#include "plugin/battery.hpp"
#include "plugin/clock.hpp"
#ifdef HAVE_WEATHER
    #include "plugin/weather.hpp"
#endif
#include "plugin/instant.hpp"
#include "plugin/password.hpp"
#include "plugin/pin.hpp"
#include "plugin/fingerprint.hpp"
#include "plugin/user.hpp"
#ifdef HAVE_PULSE
    #include "plugin/volume.hpp"
#endif
#include "plugin/mpris.hpp"
#include "plugin/network.hpp"
#include "wf-option-wrap.hpp"
#include "wf-shell-app.hpp"
#include "locker.hpp"

#define INOT_BUF_SIZE (1024 * sizeof(inotify_event))

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
    WayfireLockerApp::get().init_plugins();
    if (is_debug())
    {
        if (!m_is_locked)
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
    app->add_main_option_entry(
        [=] (const Glib::ustring & option_name,
             const Glib::ustring & value, bool has_value)
    {
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
            /* Relock logic, currently unused */
            if (can_early_wake)
            {
                prewake_signal = Glib::signal_timeout().connect([this] ()
                {
                    kill_parent(ExitType::LOCKED);

                    can_early_wake = false;
                    return G_SOURCE_REMOVE;
                }, WfOption<double>{"locker/prewake"} *1000);
            }

            perform_lock();
        } else
        {
            kill_parent(ExitType::LOCKED);
        }

        /* Called again but already screen locked. No worries */
        return;
    }

    /* Set a timer for early-wake unlock */
    WayfireShellApp::on_activate();

    if (can_early_wake)
    {
        prewake_signal = Glib::signal_timeout().connect([this] ()
        {
            kill_parent(ExitType::LOCKED);
            can_early_wake = false;
            return G_SOURCE_REMOVE;
        }, WfOption<double>{"locker/prewake"} *1000);
        /* TODO Hot config for this? */
        // exit_on_unlock = WfOption<bool>{"locker/exit_on_unlock"};
    }

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
    new CssFromConfigDouble("locker/prewake", ".fade-in {animation-name: slowfade;animation-duration: ",
        "s; animation-timing-function: linear; animation-iteration-count: 1; animation-fill-mode: forwards;} @keyframes slowfade { from {opacity:0; background: #0000;} to {opacity:1;}}");
    new CssFromConfigFont("locker/network_font", ".wf-locker .network {", "}");
    new CssFromConfigInt("locker/network_icon_size", ".wf-locker .network {-gtk-icon-size:", "px;}");

    /* Init plugins */
    plugins.emplace("clock", Plugin(new WayfireLockerClockPlugin()));
#ifdef HAVE_WEATHER
    plugins.emplace("weather", Plugin(new WayfireLockerWeatherPlugin()));
#endif
    plugins.emplace("battery", Plugin(new WayfireLockerBatteryPlugin()));
    plugins.emplace("password", Plugin(new WayfireLockerPasswordPlugin()));
    plugins.emplace("instant", (Plugin(new WayfireLockerInstantPlugin())));
    plugins.emplace("pin", Plugin(new WayfireLockerPinPlugin()));
    plugins.emplace("fingerprint", Plugin(new WayfireLockerFingerprintPlugin()));
#ifdef HAVE_PULSE
    plugins.emplace("volume", Plugin(new WayfireLockerVolumePlugin()));
#endif
    plugins.emplace("mpris", Plugin(new WayfireLockerMPRISPlugin()));
    plugins.emplace("aboutuser", Plugin(new WayfireLockerUserPlugin()));
    plugins.emplace("network", Plugin(new WayfireLockerNetworkPlugin()));

    /* Get background cache */
    char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir)
    {
        this->cache_file = std::string(xdg_runtime_dir) + "/wf-background.cache";
        std::cout << "Using cache file " << this->cache_file << std::endl;
        inotify_bg_file = inotify_init();
        Glib::signal_io().connect(
            [this] (Glib::IOCondition cond)
        {
            char buf[INOT_BUF_SIZE];
            read(inotify_bg_file, buf, INOT_BUF_SIZE);
            reload_background();
            return G_SOURCE_CONTINUE;
        },
            inotify_bg_file, Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
        reload_background();
    } else
    {
        std::cout << "Not reading from cache file " << std::endl;
    }

    /* Create plugin option callbacks */
    for (auto & it : plugins)
    {
        Plugin plugin = it.second;
        plugin->enable.set_callback(
            [plugin, this] ()
        {
            if (plugin->enable)
            {
                plugin->init();
                for (auto & it : window_list)
                {
                    int id = it.first;
                    plugin->add_output(id, it.second->grid);
                }
            } else
            {
                for (auto & it : window_list)
                {
                    int id = it.first;
                    plugin->remove_output(id, it.second->grid);
                }

                plugin->deinit();
            }
        });
        plugin->position.set_callback(
            [this, plugin] ()
        {
            for (auto & it : window_list)
            {
                int id = it.first;
                auto window = it.second;
                plugin->remove_output(id, window->grid);
                plugin->add_output(id, window->grid);
            }
        });
    }

    perform_lock();
}

/** Called just as lock starts but before window is shown */
void WayfireLockerApp::init_plugins()
{
    for (auto & it : plugins)
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
    for (auto & it : plugins)
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
    window_list.emplace(id, new WayfireLockerAppLockscreen(background_path));
    auto window = window_list[id];

    for (auto& it : plugins)
    {
        Plugin plugin = it.second;
        if (plugin->enable)
        {
            it.second->add_output(id, window->grid);
        }
    }

    windows_signals.push_back(window->signal_realize().connect(
        [this] ()
    {
        if (!can_early_wake)
        {
            windows_signals.push_back(Glib::signal_timeout().connect([this] ()
            {
                kill_parent(ExitType::LOCKED);
                return G_SOURCE_REMOVE;
            }, 100));
        }
    }, true));

    windows_signals.push_back(window->signal_close_request().connect([this, id] ()
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
    }, false));
    if (is_debug())
    {
        init_plugins();
        m_is_locked = true;
        window->present();
    } else
    {
        gtk_session_lock_instance_assign_window_to_monitor(lock, window->gobj(), monitor);
    }

    if (can_early_wake)
    {
        window->add_css_class("fade-in");
    }
}

/* Called on any successful auth to unlock & end locker */
void WayfireLockerApp::perform_unlock(std::string reason)
{
    /* Offset the actual logic so that any callbacks that call
     *  this get a chance to exit cleanly. */
    Glib::signal_idle().connect([this, reason] ()
    {
        std::cout << "Unlocked : " << reason << std::endl;
        kill_parent(ExitType::USER_UNLOCKED);
        if (m_is_debug)
        {
            /* We need to manually close in debug mode */
            for (auto & it : window_list)
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

        for (auto & it : window_list)
        {
            it.second->disconnect();
        }

        for (auto signal : windows_signals)
        {
            signal.disconnect();
        }

        if (lockout_signal)
        {
            lockout_signal.disconnect();
        }

        if (prewake_signal)
        {
            prewake_signal.disconnect();
        }

        return G_SOURCE_REMOVE;
    });
}

void WayfireLockerApp::create(int argc, char **argv, pid_t p_pid)
{
    if (instance)
    {
        throw std::logic_error("Running WayfireLockerApp twice!");
    }

    instance = std::unique_ptr<WayfireShellApp>(new WayfireLockerApp{});
    (dynamic_cast<WayfireLockerApp&>(*instance)).p_pid = p_pid;
    instance->init_app();
    instance->run(argc, argv);
    /* In case exit has happened before parent quits */
    (dynamic_cast<WayfireLockerApp&>(*instance)).kill_parent(ExitType::ERROR_NOT_LOCKED);
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
    pid_t c_pid, p_pid = getpid();
    c_pid = fork();
    if (c_pid == -1)
    {
        std::cerr << "Unable to fork, exiting" << std::endl;
        return 1;
    } else if (c_pid > 0)
    {
        signal(ExitType::LOCKED, [] (int)
        {
            std::cout << "Locked :: exit(0)" << std::endl;
            exit(0); /* Fully locked, return control */
        });
        signal(ExitType::USER_UNLOCKED, [] (int)
        {
            std::cout << "Unlocked :: exit(2)" << std::endl;
            exit(2); /* User fully authenticated or interupted before lock */
        });
        signal(ExitType::ERROR_NOT_LOCKED, [] (int)
        {
            std::cout << "Error :: exit(1)" << std::endl;
            exit(1);
        });
        sleep(12);
        std::cout << "Timed out" << std::endl;
        exit(1); /* Lock timed out */
    } else
    {
        if (!gtk_session_lock_is_supported())
        {
            std::cerr << "This session does not support locking" << std::endl;
            kill(p_pid, ExitType::ERROR_NOT_LOCKED);
            exit(1);
        }

        WayfireLockerApp::create(argc, argv, p_pid);
    }

    return 0;
}

/* lock session calllbacks */
void on_session_locked_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session locked" << std::endl;
    WayfireLockerApp::get().set_is_locked(true);
}

void on_session_lock_failed_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session lock failed" << std::endl;
    WayfireLockerApp::get().set_is_locked(false);
    WayfireLockerApp::get().kill_parent(ExitType::ERROR_NOT_LOCKED);
    exit(0);
}

void on_session_unlocked_c(GtkSessionLockInstance *lock, void *data)
{
    std::cout << "Session unlocked" << std::endl;
    WayfireLockerApp::get().set_is_locked(false);
    WayfireLockerApp::get().deinit_plugins();
    if (WayfireLockerApp::get().exit_on_unlock)
    {
        // Exiting too early causes a lock-out
        Glib::signal_idle().connect([] () -> bool
        {
            WayfireLockerApp::get().kill_parent(ExitType::USER_UNLOCKED);
            exit(0);
        }, 1);
    }

    // Lose windows, but not right away to avoid lock-screen-crash for 1 frame
    Glib::signal_idle().connect([] ()
    {
        WayfireLockerApp::get().window_list.clear();
        return G_SOURCE_REMOVE;
    });
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

void WayfireLockerApp::recieved_bad_auth()
{
    bad_auth_count++;
    if (bad_auth_count > WfOption<int>{"locker/lockout_attempts"})
    {
        // Lockout now
        lockout = true;
        for (auto & it : plugins)
        {
            it.second->lockout_changed(true);
        }

        //
        lockout_signal = Glib::signal_timeout().connect_seconds(
            [this] ()
        {
            lockout = false;
            bad_auth_count = 0;
            for (auto & it : plugins)
            {
                it.second->lockout_changed(false);
            }

            return G_SOURCE_REMOVE;
        }, WfOption<int>{"locker/lockout_timer"});
    }
}

bool WayfireLockerApp::is_locked_out()
{
    return lockout;
}

void WayfireLockerApp::reload_background()
{
    if (cache_file.length() > 1)
    {
        std::ifstream f(cache_file);
        if (f.is_open())
        {
            std::string s;
            if (getline(f, s))
            {
                std::cout << "Background " << s << std::endl;
                for (auto & it : window_list)
                {
                    auto widget = it.second;
                    widget->background.show_image(s);
                }

                background_path = s;
            }
        }
    }

    // Re add notify
    inotify_add_watch(inotify_bg_file,
        cache_file.c_str(),
        IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE);
}

void WayfireLockerApp::kill_parent(ExitType exittype)
{
    if (p_pid)
    {
        kill(p_pid, exittype); /* We are fully locked */
        p_pid = 0;
    }
}
