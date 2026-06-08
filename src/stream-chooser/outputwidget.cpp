#include <iostream>
#include <sys/mman.h>

#include "outputwidget.hpp"
#include "gdk/wayland/gdkwayland.h"
#include "stream-chooser.hpp"

/* Copy Capture Callbacks */

static void frame_handle_transform(void*,
    struct ext_image_copy_capture_frame_v1*,
    uint32_t)
{}

static void frame_handle_damage(void*,
    struct ext_image_copy_capture_frame_v1*,
    int32_t, int32_t, int32_t, int32_t)
{}

static void frame_handle_presentation_time(void*,
    struct ext_image_copy_capture_frame_v1*,
    uint32_t, uint32_t, uint32_t)
{}

static void frame_handle_ready(void *data,
    struct ext_image_copy_capture_frame_v1*)
{
    WayfireChooserOutput *output = (WayfireChooserOutput*)data;
    output->buffer_ready();
    output->frame_in_flight = false;
}

static void frame_handle_failed(void *data,
    struct ext_image_copy_capture_frame_v1 *handle,
    uint32_t reason)
{
    WayfireChooserOutput *output = (WayfireChooserOutput*)data;
    std::cerr << "Failed to copy frame because reason: " << reason << std::endl;
    ext_image_copy_capture_frame_v1_destroy(handle);
    output->frame = nullptr;
    output->frame_in_flight = false;
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener = {
    .transform = frame_handle_transform,
    .damage    = frame_handle_damage,
    .presentation_time = frame_handle_presentation_time,
    .ready  = frame_handle_ready,
    .failed = frame_handle_failed,
};

static void session_handle_buffer_size(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t width, uint32_t height)
{
    WayfireChooserOutput *output = (WayfireChooserOutput*)data;
    output->current_buffer_width  = width;
    output->current_buffer_height = height;
}

static void session_handle_shm_format(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t format)
{
    WayfireChooserOutput *output = (WayfireChooserOutput*)data;
    output->current_buffer_format = format;
}

static void session_handle_dmabuf_device(void*,
    struct ext_image_copy_capture_session_v1*,
    struct wl_array*)
{}

static void session_handle_dmabuf_format(void*,
    struct ext_image_copy_capture_session_v1*,
    uint32_t,
    struct wl_array*)
{}

static void session_handle_done(void *data,
    struct ext_image_copy_capture_session_v1*)
{}

static void session_handle_stopped(void*,
    struct ext_image_copy_capture_session_v1 *session)
{
    ext_image_copy_capture_session_v1_destroy(session);
    session = NULL;
}

static const struct ext_image_copy_capture_session_v1_listener recording_session_listener = {
    .buffer_size   = session_handle_buffer_size,
    .shm_format    = session_handle_shm_format,
    .dmabuf_device = session_handle_dmabuf_device,
    .dmabuf_format = session_handle_dmabuf_format,
    .done    = session_handle_done,
    .stopped = session_handle_stopped,
};

static void dmabuf_created(void *data, struct zwp_linux_buffer_params_v1*,
    struct wl_buffer *wl_buffer)
{
    auto output = (WayfireChooserOutput*)data;
    output->buffer->buffer = wl_buffer;
}

static void dmabuf_failed(void*, struct zwp_linux_buffer_params_v1*)
{
    std::cerr << "Failed to create dmabuf" << std::endl;
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
    .created = dmabuf_created,
    .failed  = dmabuf_failed,
};

static void frame_handle_linux_dmabuf(uint32_t width, uint32_t height, WayfireChooserOutput *output)
{
    auto format = (output->current_buffer_format == WL_SHM_FORMAT_XRGB8888) ?
        GBM_FORMAT_XRGB8888 : GBM_FORMAT_ARGB8888;

    auto buffer = output->buffer;

    if (buffer->bo)
    {
        gbm_bo_destroy(buffer->bo);
        buffer->bo = nullptr;
    }

    if (buffer->params)
    {
        zwp_linux_buffer_params_v1_destroy(buffer->params);
        buffer->params = nullptr;
    }

    auto w = width;
    auto h = height;

    const uint64_t modifier = 0; // DRM_FORMAT_MOD_LINEAR
    buffer->bo = gbm_bo_create_with_modifiers(WayfireStreamChooserApp::getInstance().gbm_device_ptr, w, h,
        format, &modifier, 1);
    if (buffer->bo == NULL)
    {
        buffer->bo = gbm_bo_create(WayfireStreamChooserApp::getInstance().gbm_device_ptr, w, h,
            format, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
    }

    if (buffer->bo == NULL)
    {
        perror("failed to create gbm bo");
        return;
    }

    buffer->width  = gbm_bo_get_width(buffer->bo);
    buffer->height = gbm_bo_get_height(buffer->bo);
    buffer->stride = gbm_bo_get_stride(buffer->bo);
    buffer->params = zwp_linux_dmabuf_v1_create_params(WayfireStreamChooserApp::getInstance().dmabuf);

    uint64_t mod = gbm_bo_get_modifier(buffer->bo);
    zwp_linux_buffer_params_v1_add(buffer->params,
        gbm_bo_get_fd(buffer->bo), 0,
        gbm_bo_get_offset(buffer->bo, 0),
        gbm_bo_get_stride(buffer->bo),
        mod >> 32, mod & 0xffffffff);

    zwp_linux_buffer_params_v1_add_listener(buffer->params, &params_listener, output);
    zwp_linux_buffer_params_v1_create(buffer->params, w, h, format, 0);
}

void WayfireChooserOutput::start_output_source_ssession()
{
    copy_capture_source = ext_output_image_capture_source_manager_v1_create_source(
        WayfireStreamChooserApp::getInstance().output_capture_manager,
        output_handle);
    recording_session = ext_image_copy_capture_manager_v1_create_session(
        WayfireStreamChooserApp::getInstance().manager,
        copy_capture_source,
        0);
    ext_image_copy_capture_session_v1_add_listener(recording_session, &recording_session_listener, this);
}

void WayfireChooserOutput::stream()
{
    streaming = true;
}

void WayfireChooserOutput::pause()
{
    streaming = false;
}

WayfireChooserOutput::WayfireChooserOutput(std::shared_ptr<Gdk::Monitor> output) : output(output)
{
    append(contents);
    append(model);
    append(connector);
    set_valign(Gtk::Align::FILL);
    set_halign(Gtk::Align::FILL);

    output_handle = gdk_wayland_monitor_get_wl_output(output->gobj());

    model.set_label(output->get_model());
    connector.set_label(output->get_connector());

    set_orientation(Gtk::Orientation::VERTICAL);

    signals.push_back(output->signal_invalidate().connect([=]
    {
        WayfireStreamChooserApp::getInstance().remove_output(output->get_connector());
    }));

    signals.push_back(WayfireStreamChooserApp::getInstance().signal_resize().connect(
        [=] (int width, int height)
    {
        set_size_request(-1, height / 3 + height * 0.075);
    }));

    buffer = std::make_shared<output_buffer>();

    start_output_source_ssession();

    initial_timeout = Glib::signal_timeout().connect(
        [this] ()
    {
        this->pause();
        return G_SOURCE_REMOVE;
    }, 2000);

    signals.push_back(
        WayfireStreamChooserApp::getInstance().screen_list.signal_selected_children_changed().connect(
            [=] ()
    {
        if (WayfireStreamChooserApp::getInstance().screen_list.get_selected_children()[0]->get_children()[0]
            ==
            this)
        {
            this->initial_timeout.disconnect();
            this->pause_timeout.disconnect();
            this->stream();
            return;
        }

        this->pause_timeout.disconnect();
        this->pause_timeout = Glib::signal_timeout().connect(
            [this] ()
        {
            this->pause();
            return G_SOURCE_REMOVE;
        }, 2000);
    }));

    auto motion_controller = Gtk::EventControllerMotion::create();
    signals.push_back(motion_controller->signal_enter().connect(
        [this] (double, double)
    {
        this->initial_timeout.disconnect();
        this->pause_timeout.disconnect();
        this->stream();
    }));
    add_controller(motion_controller);

    motion_controller = Gtk::EventControllerMotion::create();
    signals.push_back(motion_controller->signal_leave().connect(
        [this] ()
    {
        this->pause_timeout.disconnect();
        this->pause_timeout = Glib::signal_timeout().connect(
            [this] ()
        {
            if (WayfireStreamChooserApp::getInstance().screen_list.get_selected_children()[0]->get_children()[
                0] == this)
            {
                return G_SOURCE_REMOVE;
            }

            this->pause();
            return G_SOURCE_REMOVE;
        }, 2000);
    }));
    add_controller(motion_controller);
}

WayfireChooserOutput::~WayfireChooserOutput()
{
    if (frame)
    {
        ext_image_copy_capture_frame_v1_destroy(frame);
    }

    if (copy_capture_source)
    {
        ext_image_capture_source_v1_destroy(copy_capture_source);
    }

    if (recording_session)
    {
        ext_image_copy_capture_session_v1_destroy(recording_session);
    }

    if (buffer->bo)
    {
        gbm_bo_destroy(buffer->bo);
    }

    if (buffer->params)
    {
        zwp_linux_buffer_params_v1_destroy(buffer->params);
    }

    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void WayfireChooserOutput::print()
{
    std::cout << "Monitor: " << output->get_connector() << std::endl;
    exit(0);
}

void WayfireChooserOutput::frame_request()
{
    if (!streaming)
    {
        return;
    }

    if ((current_buffer_width <= 0) || (current_buffer_height <= 0))
    {
        printf("%s invalid size\n", __func__);
        return;
    }

    if (!recording_session)
    {
        return;
    }

    bool dirty = (buffer->width != current_buffer_width) || (buffer->height != current_buffer_height) ||
        !buffer->buffer;

    buffer->width  = current_buffer_width;
    buffer->height = current_buffer_height;

    if (dirty)
    {
        frame_handle_linux_dmabuf(buffer->width, buffer->height, this);
    }

    if (!buffer->buffer)
    {
        return;
    }

    if (frame_in_flight)
    {
        return;
    }

    if (frame)
    {
        ext_image_copy_capture_frame_v1_destroy(frame);
        frame = NULL;
    }

    frame = ext_image_copy_capture_session_v1_create_frame(recording_session);
    buffer->frame = frame;

    ext_image_copy_capture_frame_v1_add_listener(buffer->frame, &frame_listener, this);
    ext_image_copy_capture_frame_v1_attach_buffer(buffer->frame, buffer->buffer);
    ext_image_copy_capture_frame_v1_damage_buffer(buffer->frame, 0, 0, buffer->width, buffer->height);
    ext_image_copy_capture_frame_v1_capture(buffer->frame);
    frame_in_flight = true;
}

void WayfireChooserOutput::buffer_ready()
{
    if ((buffer == nullptr) || (buffer->buffer == nullptr))
    {
        printf("%s buffer null\n", __func__);

        return;
    }

    uint32_t stride = 0;
    void *map_data  = NULL;
    void *data = gbm_bo_map(buffer->bo, 0, 0, buffer->width, buffer->height,
        GBM_BO_TRANSFER_READ, &stride, &map_data);
    if (!data)
    {
        perror("failed to map bo");
        return;
    }

    /* buffer->data is now valid */
    std::shared_ptr<Glib::Bytes> bytes = 0;
    size_t size = stride * buffer->height;
    bytes = Glib::Bytes::create((unsigned char*)data, size);

    if (!bytes)
    {
        gbm_bo_unmap(buffer->bo, map_data);
        return;
    }

    auto builder = Gdk::MemoryTextureBuilder::create();
    builder->set_bytes(bytes);
    builder->set_width(buffer->width);
    builder->set_height(buffer->height);
    builder->set_stride(stride);
    builder->set_format(Gdk::MemoryFormat::B8G8R8A8);

    auto texture = builder->build();

    contents.set_paintable(texture);

    gbm_bo_unmap(buffer->bo, map_data);
}
