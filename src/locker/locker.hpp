#pragma once
#include <memory>
#include <gtkmm/cssprovider.h>
#include <gtk4-session-lock.h>
#include <wayland-client.h>

#include "lockscreen.hpp"
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
    bool m_is_locked    = false;
    bool instant_lock   = false;
    int window_id_count = 0;
    int bad_auth_count  = 0;
    bool lockout        = false;

    std::vector<Glib::RefPtr<Gtk::CssProvider>> css_rules;
    sigc::connection lockout_signal, prewake_signal;
  public:
    using WayfireShellApp::WayfireShellApp;
    static void create(int argc, char **argv);
    static WayfireLockerApp& get()
    {
        return (WayfireLockerApp&)WayfireShellApp::get();
    }
    Gio::Application::Flags get_extra_application_flags() override;
    std::string get_application_name() override;
    void command_line() override;

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to create() */
    void on_monitor_present(GdkMonitor *monitor);
    void on_activate() override;
    bool is_debug()
    {
        return m_is_debug;
    }

    bool exit_on_unlock = true;


    bool is_locked();
    void set_is_locked(bool locked);
    WayfireLockerApp();
    ~WayfireLockerApp();

    Plugin get_plugin(std::string name);

    /* Give commands to compositor about lock state, or emulate them*/
    void perform_unlock(std::string reason);
    void perform_lock();
    void init_plugins();
    void deinit_plugins();
    bool can_early_wake = true;
    void user_activity();

    void recieved_bad_auth();
    bool is_locked_out();

    std::map<int, std::shared_ptr<WayfireLockerAppLockscreen>> window_list;
};
