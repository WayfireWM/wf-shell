#pragma once

#include <memory>
#include <string>
#include <wayfire/config/config-manager.hpp>

#include <gtkmm/application.h>
#include <gdkmm/monitor.h>
#include <gtkmm/cssprovider.h>

#include "wayfire-shell-unstable-v2-client-protocol.h"

using GMonitor = Glib::RefPtr<Gdk::Monitor>;

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

    WayfireOutput(const GMonitor& monitor, zwf_shell_manager_v2 *zwf_manager);
    ~WayfireOutput();
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
    std::vector<Glib::RefPtr<Gtk::CssProvider>> css_rules;

  protected:
    /** This should be initialized by the subclass in each program which uses
     * wf-shell-app */
    bool alternative_monitors = false; /* Used to skip monitor management in lockscreen */
    static std::unique_ptr<WayfireShellApp> instance;
    std::optional<std::string> cmdline_config;
    std::optional<std::string> cmdline_css;

    Glib::RefPtr<Gtk::Application> app;

    void output_list_updated(int pos, int rem, int add);
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
    zwf_shell_manager_v2 *wf_shell_manager = nullptr;

    WayfireShellApp();
    virtual ~WayfireShellApp();

    virtual std::string get_config_file();
    virtual std::string get_css_config_dir();
    virtual void run(int argc, char **argv);

    virtual void on_config_reload()
    {}
    void on_css_reload();
    void clear_css_rules();
    void add_css_file(std::string file, int priority);


    /**
     * WayfireShellApp is a singleton class.
     * Using this function, any part of the application can get access to the
     * shell app.
     */
    static WayfireShellApp& get();
};
