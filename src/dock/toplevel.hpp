#ifndef WF_DOCK_TOPLEVEL_HPP
#define WF_DOCK_TOPLEVEL_HPP

#include <memory>
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>

enum WfToplevelState
{
    WF_TOPLEVEL_STATE_ACTIVATED = (1 << 0),
    WF_TOPLEVEL_STATE_MAXIMIZED = (1 << 1),
    WF_TOPLEVEL_STATE_MINIMIZED = (1 << 2),
};

/* Represents a single opened toplevel window.
 * It displays the window icon on all outputs' docks that it is visible on */
class WfToplevel
{
    public:
    WfToplevel(zwlr_foreign_toplevel_handle_v1 *handle);
    ~WfToplevel();

    void handle_output_leave(wl_output *output);

    class impl;
    private:
    std::unique_ptr<impl> pimpl;
};

#endif /* end of include guard: WF_DOCK_TOPLEVEL_HPP */
