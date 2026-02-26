#pragma once

#include <gtkmm.h>

#include "../../widget.hpp"
#include "toplevel.hpp"
#include "layout.hpp"
#include "wf-ipc.hpp"

class WayfireWindowListOutput
{
  public:
    wl_output *output;
    std::string name;
};

class WayfireToplevel;

class WayfireWindowList : public Gtk::Box, public WayfireWidget, public IIPCSubscriber
{
    WfOption<int> user_size{"panel/window_list_size"};
    std::shared_ptr<WayfireWindowListLayout> layout;

  public:
    std::map<zwlr_foreign_toplevel_handle_v1*,
        std::unique_ptr<WayfireToplevel>> toplevels;

    wl_display *display;
    wl_registry *registry;
    wl_shm *shm;
    zwlr_foreign_toplevel_manager_v1 *manager = NULL;
    zwlr_screencopy_manager_v1 *screencopy_manager = NULL;
    WayfireOutput *output;
    Gtk::ScrolledWindow scrolled_window;

    uint32_t foreign_toplevel_manager_id;
    uint32_t foreign_toplevel_version;

    WayfireWindowList(WayfireOutput *output);
    virtual ~WayfireWindowList();
    std::unique_ptr<WayfireWindowListOutput> wayfire_window_list_output;

    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager);
    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle);
    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle);

    wayfire_config *get_config();

    void init(Gtk::Box *container) override;

    /**
     * Set the widget which should always be rendered on top of the other child
     * widgets */
    void set_top_widget(Gtk::Widget *top = nullptr);
    /** Set the absolute position of the top widget */
    void set_top_x(int x);

    /**
     * @param x the x-axis position, relative to ref
     * @param ref The widget that x is relative to. ref must be a child
     * widget of this box
     * @return the position x, but relative to the box
     */
    int get_absolute_position(int x, Gtk::Widget& ref);

    /** Find the direct child widget at the given box-relative coordinates,
     * ignoring the top widget if possible, i.e if the top widget and some
     * other widget are at the given coordinates, then the bottom widget will
     * be returned
     *
     * @return The direct child widget or none if it doesn't exist
     */
    Gtk::Widget *get_widget_at(int x);
    /** Find the direct child widget before the given box-relative coordinates,
     * ignoring the top widget if possible, i.e if the top widget and some
     * other widget are at the given coordinates, then the bottom widget will
     * be returned
     *
     * @return The direct child widget or none if it doesn't exist
     */
    Gtk::Widget *get_widget_before(int x);

    void handle_new_wl_output(void *data, wl_registry *registry, uint32_t name, const char *interface,
        uint32_t version, wl_output *output);
    void on_event(wf::json_t data) override;
    std::shared_ptr<IPCClient> ipc_client;
    bool live_window_preview_tooltips    = false;
    uint64_t live_window_preview_view_id = 0;

  private:
    int get_default_button_width();
    int get_target_button_width();
};
