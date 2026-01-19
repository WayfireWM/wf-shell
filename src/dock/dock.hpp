#pragma once

#include <gtkmm/box.h>
#include <wayland-client.h>
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>
#include <wf-option-wrap.hpp>

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

class WfDockApp : public WayfireShellApp
{
  public:
    WfDock *dock_for_wl_output(wl_output *output);
    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager);
    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle);
    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle);

    static WfDockApp& get();

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to run() */
    static void create(int argc, char **argv);
    virtual ~WfDockApp();

    void on_activate() override;
    void handle_new_output(WayfireOutput *output) override;
    void handle_output_removed(WayfireOutput *output) override;

  private:
    WfDockApp();

    class impl;
    std::unique_ptr<impl> priv;
};
