#pragma once

#include <memory>
#include <gtkmm/box.h>
#include <gtkmm/picture.h>
#include <cairomm/refptr.h>
#include <cairomm/context.h>
#include <wlr-foreign-toplevel-management-unstable-v1-client-protocol.h>
#include <ext-foreign-toplevel-list-v1-client-protocol.h>
#include <ext-image-capture-source-v1-client-protocol.h>
#include <ext-image-copy-capture-v1-client-protocol.h>
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
    ext_foreign_toplevel_handle_v1 *ext_handle = NULL;
    ext_image_copy_capture_frame_v1 *frame     = NULL;
    ext_image_capture_source_v1 *copy_capture_source     = NULL;
    ext_image_copy_capture_session_v1 *recording_session = NULL;
    bool frame_in_flight = false;
    bool timer_continue  = true;
    uint32_t current_buffer_format = GBM_FORMAT_ARGB8888;
    uint32_t current_buffer_width = 0, width = -1;
    uint32_t current_buffer_height = 0, height = -1;
    uint32_t stride;
    size_t size = 0;

#ifdef HAVE_DMABUF
    gbm_bo *bo = nullptr;
    zwp_linux_buffer_params_v1 *params = nullptr;
    void *dmabuf_data = nullptr;
    void *map_data = nullptr;
#endif // HAVE_DMABUF

    TooltipMedia(WayfireWindowList *window_list, ext_foreign_toplevel_handle_v1 *ext_handle);
    ~TooltipMedia();

    void start_toplevel_source_session();
    void request_next_frame();
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
    void set_list_toplevel_handle(ext_foreign_toplevel_handle_v1 *handle);

    class impl;

  private:
    std::unique_ptr<impl> pimpl;
};
