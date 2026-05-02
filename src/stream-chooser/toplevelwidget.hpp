#pragma once
#include <gtkmm.h>
#include <memory>
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "toplevellayout.hpp"

struct toplevel_buffer
{
    int width  = 0;
    int height = 0;
    void *data;
    wl_buffer *buffer;
    size_t size = 0;
    ext_image_copy_capture_frame_v1 *frame = NULL;
};

class WayfireChooserTopLevel : public Gtk::Box
{
  private:
    Gtk::Overlay overlay;
    Gtk::Image icon;
    Gtk::Picture screenshot;
    Gtk::Label label;

    std::string buffered_title = "", title = "";
    std::string buffered_app_id = "", app_id = "";
    std::string buffered_identifier = "", identifier = "";

    std::shared_ptr<toplevel_buffer> buffer = nullptr;
    void request_frame();
    ext_image_copy_capture_frame_v1 *frame = NULL;
    ext_image_copy_capture_session_v1 *recording_session = NULL;
    Glib::RefPtr<ToplevelLayout> layout;

  public:
    int current_buffer_width = 0, current_buffer_height = 0, current_buffer_format = 0;
    ext_foreign_toplevel_handle_v1 *handle = nullptr;
    WayfireChooserTopLevel(ext_foreign_toplevel_handle_v1 *handle);
    ~WayfireChooserTopLevel();
    void commit();
    void set_app_id(std::string app_id);
    void set_title(std::string title);
    void set_identifier(std::string identifier);
    void grab_toplevel_screenshot();
    void size();
    void buffer_ready();
    void print();
};
