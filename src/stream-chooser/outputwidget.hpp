#pragma once
#include <gbm.h>
#include "gtkmm/picture.h"
#include <gtkmm.h>
#include <gdkmm.h>
#include <wayland-client-protocol.h>
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"

struct output_buffer
{
    int width  = 0;
    int height = 0;
    int stride = 0;
    gbm_bo *bo = nullptr;
    wl_buffer *buffer = nullptr;
    zwp_linux_buffer_params_v1 *params     = nullptr;
    ext_image_copy_capture_frame_v1 *frame = NULL;
};

class WayfireChooserOutput : public Gtk::Box
{
    Gtk::Label connector, model;
    Gtk::Picture contents;

    wl_output *output_handle;
    std::shared_ptr<Gdk::Monitor> output;
    ext_image_capture_source_v1 *copy_capture_source = NULL;
    void start_output_source_ssession();
    sigc::connection timer_connection;

  public:
    ext_image_copy_capture_session_v1 *recording_session = NULL;
    std::shared_ptr<output_buffer> buffer  = nullptr;
    ext_image_copy_capture_frame_v1 *frame = NULL;
    bool frame_in_flight = false;
    void print();
    void size();
    void buffer_ready();

    WayfireChooserOutput(std::shared_ptr<Gdk::Monitor> output);
    ~WayfireChooserOutput();
    int current_buffer_width = 0, current_buffer_height = 0, current_buffer_format = 0;
};
