#pragma once

#include <memory>
#include <gtkmm/box.h>
#include <gtkmm/picture.h>
#include <cairomm/refptr.h>
#include <cairomm/context.h>
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>
#include <wlr-screencopy-client-protocol.h>
#include <wayland-client-protocol.h>
#include <wf-option-wrap.hpp>
#include "wf-shell-app.hpp"
#include "panel.hpp"

#ifdef HAVE_DMABUF
    #include <gbm.h>
    #include <xf86drm.h>
    #include <linux-dmabuf-unstable-v1-client-protocol.h>
#endif // HAVE_DMABUF

class WayfireWindowList;
class WayfireWindowListBox;

enum WayfireToplevelState
{
    WF_TOPLEVEL_STATE_ACTIVATED = (1 << 0),
    WF_TOPLEVEL_STATE_MAXIMIZED = (1 << 1),
    WF_TOPLEVEL_STATE_MINIMIZED = (1 << 2),
};

class TooltipMedia : public Gtk::Picture
{
  public:
    WayfireWindowList *window_list = nullptr;
    wl_shm *shm = nullptr;
    wl_buffer *buffer = nullptr;
    void *shm_data    = nullptr;
    zwlr_screencopy_frame_v1 *frame = nullptr;
    uint32_t buffer_width;
    uint32_t buffer_height;
    uint32_t buffer_stride;
    size_t size = 0;

#ifdef HAVE_DMABUF
    gbm_bo *bo = nullptr;
    zwp_linux_buffer_params_v1 *params = nullptr;
    void *dmabuf_data = nullptr;
    void *map_data    = nullptr;
#endif // HAVE_DMABUF

    TooltipMedia(WayfireWindowList *window_list);
    ~TooltipMedia();

    bool request_next_frame();
};

/* Represents a single opened toplevel window.
 * It displays the window icon on all outputs' docks that it is visible on */
class WayfireToplevel
{
  public:
    WayfireToplevel(WayfireWindowList *window_list, zwlr_foreign_toplevel_handle_v1 *handle);

    uint32_t get_state();
    void send_rectangle_hint();
    std::vector<zwlr_foreign_toplevel_handle_v1*>& get_children();
    ~WayfireToplevel();
    void set_hide_text(bool hide_text);
    void set_tooltip_media();
    void unset_tooltip_media();

    class impl;

  private:
    std::unique_ptr<impl> pimpl;
};
