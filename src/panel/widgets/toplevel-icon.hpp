#ifndef WF_DOCK_TOPLEVEL_ICON_HPP
#define WF_DOCK_TOPLEVEL_ICON_HPP

#include "display.hpp"
#include <memory>
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>

class WayfireToplevelIcon
{
    public:
    WayfireToplevelIcon(zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output);
    ~WayfireToplevelIcon();
    void set_app_id(std::string app_id);
    void set_title(std::string title);
    void set_state(uint32_t state);

    class impl;
    private:
    std::unique_ptr<impl> pimpl;
};

#endif /* end of include guard: WF_DOCK_TOPLEVEL_ICON_HPP */
