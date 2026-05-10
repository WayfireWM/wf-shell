#pragma once
#include "gtkmm/picture.h"
#include <gtkmm.h>
#include <gdkmm.h>
#include <wayland-client-protocol.h>
#include "ext-image-copy-capture-v1-client-protocol.h"
struct output_buffer
{
    int width  = 0;
    int height = 0;
    void *data;
    wl_buffer *buffer;
    size_t size = 0;
    ext_image_copy_capture_frame_v1 *frame = NULL;
};

class WayfireChooserOutput : public Gtk::Box
{
    Gtk::Label connector, model;
    Gtk::Picture contents;

    wl_output *output_handle;
    std::shared_ptr<Gdk::Monitor> output;
    std::shared_ptr<output_buffer> buffer  = nullptr;
    ext_image_copy_capture_frame_v1 *frame = NULL;
    ext_image_copy_capture_session_v1 *recording_session = NULL;
    void grab_output_screenshot();

  public:
    void print();
    void size();
    void buffer_ready();

    WayfireChooserOutput(std::shared_ptr<Gdk::Monitor> output);
    int current_buffer_width = 0, current_buffer_height = 0, current_buffer_format = 0;
};
