#ifndef WF_SHELL_APP_HPP
#define WF_SHELL_APP_HPP

#include <memory>
#include <set>
#include <string>
#include <wayfire/config/config-manager.hpp>

#include <gtkmm/application.h>
#include <gdkmm/monitor.h>

#include "wayfire-shell-unstable-v2-client-protocol.h"

using GMonitor = Glib::RefPtr<Gdk::Monitor>;

struct WayfireShellManager; 
using ShellManager = std::shared_ptr<WayfireShellManager>;

/**
 * Represents a single output
 */
struct WayfireOutput
{
    GMonitor monitor;
    wl_output *wo;
    zwf_output_v2 *output;
    sigc::signal<void()> toggle_menu_signal();
    sigc::signal<void()> m_toggle_menu_signal;

    WayfireOutput(const GMonitor& monitor,  ShellManager shell_manager);
    ~WayfireOutput();
};

struct WayfireShellManager
{
    WayfireShellManager(zwf_shell_manager_v2 *wf_shell_manager);
    zwf_output_v2* get_wf_output(wl_output* output);
    zwf_keyboard_lang_manager_v2* get_keyboard_lang_manager();
  private:
    zwf_shell_manager_v2 *wf_shell_manager;
};

/**
 * A basic shell application.
 *
 * It is suitable for applications that need to show one or more windows
 * per monitor.
 */
class WayfireShellApp
{
  private:
    std::vector<std::unique_ptr<WayfireOutput>> monitors;

  protected:
    /** This should be initialized by the subclass in each program which uses
     * wf-shell-app */
    static std::unique_ptr<WayfireShellApp> instance;
    std::optional<std::string> cmdline_config;
    std::optional<std::string> cmdline_css;

    Glib::RefPtr<Gtk::Application> app;

    virtual void add_output(GMonitor monitor);
    virtual void rem_output(GMonitor monitor);

    /* The following functions can be overridden in the shell implementation to
     * handle the events */
    virtual void on_activate();
    virtual bool parse_cfgfile(const Glib::ustring & option_name,
        const Glib::ustring & value, bool has_value);
    virtual bool parse_cssfile(const Glib::ustring & option_name,
        const Glib::ustring & value, bool has_value);
    virtual void handle_new_output(WayfireOutput *output)
    {}
    virtual void handle_output_removed(WayfireOutput *output)
    {}

  public:
    int inotify_fd;
    int inotify_css_fd;
    wf::config::config_manager_t config;
    ShellManager wf_shell_manager;

    WayfireShellApp(int argc, char **argv);
    virtual ~WayfireShellApp();

    virtual std::string get_config_file();
    virtual std::string get_css_config_dir();
    virtual void run();

    virtual void on_config_reload()
    {}

    virtual void on_css_reload()
    {}

    /**
     * WayfireShellApp is a singleton class.
     * Using this function, any part of the application can get access to the
     * shell app.
     */
    static WayfireShellApp& get();
};

#endif /* end of include guard: WF_SHELL_APP_HPP */
