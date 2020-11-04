#include "toplevel.hpp"
#include "toplevel-icon.hpp"
#include "dock.hpp"
#include <cassert>

namespace
{
    extern zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl;
}

class WfToplevel::impl
{
    zwlr_foreign_toplevel_handle_v1 *handle;
    std::map<wl_output*, std::unique_ptr<WfToplevelIcon>> icons;
    std::string _title, _app_id;
    uint32_t _state = 0;

    public:
    impl(zwlr_foreign_toplevel_handle_v1* handle)
    {
        this->handle = handle;
        zwlr_foreign_toplevel_handle_v1_add_listener(handle,
            &toplevel_handle_v1_impl, this);
    }

    void handle_output_enter(wl_output *output)
    {
        if (icons.count(output))
            return;

        auto dock = WfDockApp::get().dock_for_wl_output(output);

        /* This catches two edge cases:
         * 1. The dock on the given output simply was closed by the user
         *
         * 2. The wl_output has been bound multiple times - this happens because
         * gtk will bind each output once, and then we bind it second time. So
         * the compositor will actually send the output_enter/leave at least
         * twice, and the one time when we get it with the output resource bound
         * by gtk, we need to ignore the request */
        if (!dock)
            return;

        auto icon = std::unique_ptr<WfToplevelIcon>(
            new WfToplevelIcon(handle, output));

        icon->set_title(_title);
        icon->set_app_id(_app_id);
        icon->set_state(_state);

        icons[output] = std::move(icon);
    }

    void handle_output_leave(wl_output *output)
    {
        icons.erase(output);
    }

    void set_title(std::string title)
    {
        _title = title;
        for (auto& icon : icons)
            icon.second->set_title(title);
    }

    void set_app_id(std::string app_id)
    {
        _app_id = app_id;
        for (auto& icon : icons)
            icon.second->set_app_id(app_id);
    }

    void set_state(uint32_t state)
    {
        _state = state;
        for (auto& icon : icons)
            icon.second->set_state(state);
    }
};


WfToplevel::WfToplevel(zwlr_foreign_toplevel_handle_v1 *handle)
    :pimpl(new WfToplevel::impl(handle)) { }
WfToplevel::~WfToplevel() = default;

void WfToplevel::handle_output_leave(wl_output *output)
{
    pimpl->handle_output_leave(output);
}

using toplevel_t = zwlr_foreign_toplevel_handle_v1*;
static void handle_toplevel_title(void *data, toplevel_t, const char *title)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->set_title(title);
}

static void handle_toplevel_app_id(void *data, toplevel_t, const char *app_id)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->set_app_id(app_id);
}

static void handle_toplevel_output_enter(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->handle_output_enter(output);
}

static void handle_toplevel_output_leave(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->handle_output_leave(output);
}

/* wl_array_for_each isn't supported in C++, so we have to manually
 * get the data from wl_array, see:
 *
 * https://gitlab.freedesktop.org/wayland/wayland/issues/34 */
template<class T>
static void array_for_each(wl_array *array, std::function<void(T)> func)
{
    assert(array->size % sizeof(T) == 0); // do not use malformed arrays
    for (T* entry = (T*)array->data; (char*)entry < ((char*)array->data + array->size); entry++)
    {
        func(*entry);
    }
}

static void handle_toplevel_state(void *data, toplevel_t, wl_array *state)
{
    uint32_t flags = 0;
    array_for_each<uint32_t> (state, [&flags] (uint32_t st)
    {
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            flags |= WF_TOPLEVEL_STATE_ACTIVATED;
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
            flags |= WF_TOPLEVEL_STATE_MAXIMIZED;
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            flags |= WF_TOPLEVEL_STATE_MINIMIZED;
    });

    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->set_state(flags);
}

static void handle_toplevel_done(void *data, toplevel_t)
{
//    auto impl = static_cast<WfToplevel::impl*> (data);
}

static void handle_toplevel_closed(void *data, toplevel_t handle)
{
    WfDockApp::get().handle_toplevel_closed(handle);
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}

namespace
{
struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl = {
    .title        = handle_toplevel_title,
    .app_id       = handle_toplevel_app_id,
    .output_enter = handle_toplevel_output_enter,
    .output_leave = handle_toplevel_output_leave,
    .state        = handle_toplevel_state,
    .done         = handle_toplevel_done,
    .closed       = handle_toplevel_closed
};
}
