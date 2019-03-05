#ifndef WF_PANEL_HPP
#define WF_PANEL_HPP

#include <memory>
#include <wayland-client.h>
#include <gtkmm/window.h>

#include "config.hpp"
#include "display.hpp"

class WayfirePanel
{
    public:
    WayfirePanel(WayfireOutput *output);

    wl_surface *get_wl_surface();
    Gtk::Window& get_window();
    void handle_config_reload();

    private:
    class impl;
    std::unique_ptr<impl> pimpl;
};

class WayfirePanelApp
{
    public:
    WayfirePanel* panel_for_wl_output(wl_output *output);
    WayfireDisplay *get_display();
    wayfire_config *get_config();

    static WayfirePanelApp& get();

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to run() */
    static void run(int argc, char **argv);
    ~WayfirePanelApp();

    private:
    WayfirePanelApp(int argc, char **argv);

    class impl;
    std::unique_ptr<impl> pimpl;
    static std::unique_ptr<WayfirePanelApp> instance;
};

#endif /* end of include guard: WF_PANEL_HPP */

