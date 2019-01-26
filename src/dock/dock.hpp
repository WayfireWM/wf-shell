#ifndef WF_DOCK_HPP
#define WF_DOCK_HPP

#include <map>
#include <gtkmm/hvbox.h>
#include <wayland-client.h>

#include "toplevel-icon.hpp"
#include "wf-shell-app.hpp"

class WfDock
{
    public:
    WfDock(WayfireOutput *output);
    ~WfDock();

    void add_child(Gtk::Widget& widget);
    void rem_child(Gtk::Widget& widget);

    wl_surface *get_wl_surface();
    class impl;
    private:
    std::unique_ptr<impl> pimpl;
};

class WfDockApp
{
    public:
    WfDock* dock_for_wl_output(wl_output *output);
    WayfireDisplay *get_display();
    wayfire_config *get_config();

    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager);
    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1* handle);
    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle);

    static WfDockApp& get();

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to run() */
    static void run(int argc, char **argv);
    ~WfDockApp();

    private:
    WfDockApp(int argc, char **argv);

    class impl;
    std::unique_ptr<impl> pimpl;
    static std::unique_ptr<WfDockApp> instance;
};

#endif /* end of include guard: WF_DOCK_HPP */
