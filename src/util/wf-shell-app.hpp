#ifndef WF_SHELL_APP_HPP
#define WF_SHELL_APP_HPP

#include <set>
#include <string>
#include <wayfire/config/config-manager.hpp>

#include <gtkmm/application.h>
#include <gdkmm/monitor.h>

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

  protected:
    /** This should be initialized by the subclass in each program which uses
     * wf-shell-app */
    static std::unique_ptr<WayfireShellApp> instance;

    Glib::RefPtr<Gtk::Application> app;

    virtual void add_output(GMonitor monitor);
    virtual void rem_output(GMonitor monitor);

    /* The following functions can be overridden in the shell implementation to
     * handle the events */
    virtual void on_activate();
    virtual void handle_new_output(WayfireOutput *output) {}
    virtual void handle_output_removed(WayfireOutput *output) {}

  public:
    int inotify_fd;
    wf::config::config_manager_t config;
    zwf_shell_manager_v2 *manager = nullptr;

    WayfireShellApp(int argc, char **argv);
    virtual ~WayfireShellApp();

    virtual std::string get_config_file();
    virtual void run();

    virtual void on_config_reload() {}

    /**
     * WayfireShellApp is a singleton class.
     * Using this function, any part of the application can get access to the
     * shell app.
     */
    static WayfireShellApp& get();
};

#endif /* end of include guard: WF_SHELL_APP_HPP */
