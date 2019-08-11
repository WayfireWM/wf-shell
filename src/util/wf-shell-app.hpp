#ifndef WF_SHELL_APP_HPP
#define WF_SHELL_APP_HPP

#include <set>
#include <string>
#include <config.hpp>

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
    Glib::RefPtr<Gtk::Application> app;
    virtual ~WayfireShellApp() {};

    virtual void on_activate();

    virtual void add_output(GMonitor monitor);
    virtual void rem_output(GMonitor monitor);

  public:

    int inotify_fd;
    std::unique_ptr<wayfire_config> config;
    zwf_shell_manager_v2 *manager = nullptr;

    WayfireShellApp(int argc, char **argv);

    virtual std::string get_config_file();
    virtual void on_config_reload() {}

    virtual void handle_new_output(WayfireOutput *output) {}
    virtual void handle_output_removed(WayfireOutput *output) {}

    virtual void run();
};

#endif /* end of include guard: WF_SHELL_APP_HPP */
