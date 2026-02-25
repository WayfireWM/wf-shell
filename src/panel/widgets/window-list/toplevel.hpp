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
    WayfireWindowList *window_list = NULL;
    wl_shm *shm = NULL;
    wl_buffer *buffer = NULL;
    void *shm_data    = NULL;
    char *screencopy_data = NULL;
    zwlr_screencopy_frame_v1 *frame = NULL;
    zwlr_screencopy_manager_v1 *screencopy_manager = NULL;

    int buffer_width  = 200;
    int buffer_height = 200;
    int buffer_stride = 200 * 4;
    size_t size = 0;

    TooltipMedia(WayfireWindowList *window_list);
    ~TooltipMedia()
    {}

    bool on_tick(const Glib::RefPtr<Gdk::FrameClock>& clock);
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

    class impl;

  private:
    std::unique_ptr<impl> pimpl;
};
