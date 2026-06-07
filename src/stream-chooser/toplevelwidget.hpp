#pragma once
#include <gbm.h>
#include <gtkmm.h>
#include <memory>
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "toplevellayout.hpp"

struct toplevel_buffer
{
    int width  = 0;
    int height = 0;
    int stride = 0;
    gbm_bo *bo = nullptr;
    wl_buffer *buffer = nullptr;
    zwp_linux_buffer_params_v1 *params     = nullptr;
    ext_image_copy_capture_frame_v1 *frame = NULL;
};

class WayfireChooserTopLevel : public Gtk::Box
{
  private:
    Gtk::Overlay overlay;
    Gtk::Image icon;
    Gtk::Label label;

    std::string buffered_title = "", title = "";
    std::string buffered_app_id = "", app_id = "";
    std::string buffered_identifier = "", identifier = "";
    Glib::RefPtr<ToplevelLayout> layout;

    ext_image_copy_capture_session_v1 *recording_session = NULL;
    ext_image_capture_source_v1 *copy_capture_source = NULL;

  public:
    Gtk::Picture screenshot;
    int current_buffer_width = 0, current_buffer_height = 0, current_buffer_format = 0;
    ext_foreign_toplevel_handle_v1 *handle = nullptr;
    std::shared_ptr<toplevel_buffer> buffer = nullptr;
    ext_image_copy_capture_frame_v1 *frame = NULL;
    bool on_frame_tick(const Glib::RefPtr<Gdk::FrameClock>& frame_clock);
    WayfireChooserTopLevel(ext_foreign_toplevel_handle_v1 *handle);
    ~WayfireChooserTopLevel();
    void commit();
    void destroy();
    void set_app_id(std::string app_id);
    void set_title(std::string title);
    void set_identifier(std::string identifier);
    void grab_toplevel_screenshot();
    void size();
    void buffer_ready();
    void print();
};
