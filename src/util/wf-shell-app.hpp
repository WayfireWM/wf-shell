#ifndef WF_SHELL_APP_HPP
#define WF_SHELL_APP_HPP

#include <string>
#include <config.hpp>

#include <gtkmm/application.h>

#include "display.hpp"

/* A base class for applications like backgrounds, panels, docks, etc */
class WayfireShellApp
{
    protected:
        Glib::RefPtr<Gtk::Application> app;
        virtual ~WayfireShellApp() {};

        virtual void on_activate();

    public:
        int inotify_fd;

        std::unique_ptr<wayfire_config> config;
        std::unique_ptr<WayfireDisplay> display;

        WayfireShellApp(int argc, char **argv);

        virtual std::string get_config_file();
        virtual void on_config_reload() {}

        virtual void on_new_output(WayfireOutput *output) {}
        virtual void on_output_removed(WayfireOutput *output) {}

        virtual void run();
};

#endif /* end of include guard: WF_SHELL_APP_HPP */
