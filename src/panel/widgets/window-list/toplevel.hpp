#ifndef WAYFIRE_PANEL_TOPLEVEL_HPP
#define WAYFIRE_PANEL_TOPLEVEL_HPP

#include <memory>
#include <gtkmm/box.h>
#include <cairomm/refptr.h>
#include <cairomm/context.h>
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>

#include "window-list.hpp"

class WayfireWindowList;
class WayfireWindowListBox;

enum WayfireToplevelState
{
    WF_TOPLEVEL_STATE_ACTIVATED = (1 << 0),
    WF_TOPLEVEL_STATE_MAXIMIZED = (1 << 1),
    WF_TOPLEVEL_STATE_MINIMIZED = (1 << 2),
};

/* Represents a single opened toplevel window.
 * It displays the window icon on all outputs' docks that it is visible on */
class WayfireToplevel
{
    public:
    WayfireToplevel(WayfireWindowList *window_list, zwlr_foreign_toplevel_handle_v1 *handle);

    uint32_t get_state();
    void set_width(int pixels);
    zwlr_foreign_toplevel_handle_v1 * get_parent();
    void set_parent(zwlr_foreign_toplevel_handle_v1 *);
    std::vector<zwlr_foreign_toplevel_handle_v1 *>& get_children();
    ~WayfireToplevel();

    class impl;
    private:
    std::unique_ptr<impl> pimpl;
};

#endif /* end of include guard: WAYFIRE_PANEL_TOPLEVEL_HPP */
