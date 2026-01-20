#pragma once
#include <memory>
#include <gtkmm/window.h>
#include <gtkmm/cssprovider.h>
#include <gtk4-session-lock.h>
#include <wayland-client.h>

#include "wf-shell-app.hpp"
#include "plugin.hpp"

using Plugin = std::shared_ptr<WayfireLockerPlugin>;
void on_session_locked_c(GtkSessionLockInstance *lock, void *data);
void on_session_lock_failed_c(GtkSessionLockInstance *lock, void *data);
void on_session_unlocked_c(GtkSessionLockInstance *lock, void *data);
void on_monitor_present_c(GtkSessionLockInstance *lock, GdkMonitor *monitor, void *data);

class WayfireLockerApp : public WayfireShellApp
{
  private:
    WayfireLockerApp(WayfireLockerApp const& copy);
    WayfireLockerApp& operator =(WayfireLockerApp const& copy);

    std::string get_config_file() override;

    GtkSessionLockInstance *lock;
    std::map<std::string, Plugin> plugins = {};

    bool m_is_debug     = false;
    int window_id_count = 0;

    std::vector<Glib::RefPtr<Gtk::CssProvider>> css_rules;

  public:
    using WayfireShellApp::WayfireShellApp;
    static void create(int argc, char **argv);
    static WayfireLockerApp& get()
    {
        return (WayfireLockerApp&)WayfireShellApp::get();
    }

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to create() */
    void on_monitor_present(GdkMonitor *monitor);
    void on_activate() override;
    bool is_debug()
    {
        return m_is_debug;
    }

    ~WayfireLockerApp();

    Plugin get_plugin(std::string name);
    void unlock();

  private:
};
