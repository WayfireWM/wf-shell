#include <iostream>

#include <sys/mman.h>
#include <gdk/wayland/gdkwayland.h>
#include "ext-image-capture-source-v1-client-protocol.h"
#include "glib.h"
#include "glibmm/main.h"
#include "gtk-utils.hpp"
#include "stream-chooser.hpp"
#include "toplevellayout.hpp"
#include "toplevelwidget.hpp"

/* Toplevel Callbacks */

static void toplevel_handle_closed(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    WayfireStreamChooserApp::getInstance().remove_toplevel(toplevel);
    ext_foreign_toplevel_handle_v1_destroy(handle);
}

static void toplevel_handle_done(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->commit();
}

static void toplevel_handle_title(void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *title)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->set_title(title);
}

static void toplevel_handle_app_id(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle1,
    const char *app_id)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->set_app_id(app_id);
}

static void toplevel_handle_identifier(void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *identifier)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->set_identifier(identifier);
}

ext_foreign_toplevel_handle_v1_listener listener =
{
    .closed = toplevel_handle_closed,
    .done   = toplevel_handle_done,
    .title  = toplevel_handle_title,
    .app_id = toplevel_handle_app_id,
    .identifier = toplevel_handle_identifier,
};

static void session_handle_buffer_size(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t width, uint32_t height)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->current_buffer_width  = width;
    toplevel->current_buffer_height = height;
}

static void session_handle_shm_format(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t format)
{}

static void session_handle_dmabuf_device(void*,
    struct ext_image_copy_capture_session_v1*,
    struct wl_array*)
{}

static void session_handle_dmabuf_format(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t format,
    struct wl_array*)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->current_buffer_format = format;
}

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
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->buffer_ready();
    toplevel->frame_in_flight = false;
}

static void frame_handle_failed(void *data,
    struct ext_image_copy_capture_frame_v1 *handle,
    uint32_t reason)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    std::cerr << "Failed to copy frame because reason: " << reason << std::endl;
    ext_image_copy_capture_frame_v1_destroy(handle);
    toplevel->frame = nullptr;
    toplevel->frame_in_flight = false;
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener = {
    .transform = frame_handle_transform,
    .damage    = frame_handle_damage,
    .presentation_time = frame_handle_presentation_time,
    .ready  = frame_handle_ready,
    .failed = frame_handle_failed,
};

static void dmabuf_created(void *data, struct zwp_linux_buffer_params_v1*,
    struct wl_buffer *wl_buffer)
{
    auto toplevel = (WayfireChooserTopLevel*)data;
    toplevel->buffer->buffer = wl_buffer;
}

static void dmabuf_failed(void*, struct zwp_linux_buffer_params_v1*)
{
    std::cerr << "Failed to create dmabuf" << std::endl;
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
    .created = dmabuf_created,
    .failed  = dmabuf_failed,
};

static void frame_handle_linux_dmabuf(uint32_t width, uint32_t height, WayfireChooserTopLevel *toplevel)
{
    auto format = (toplevel->current_buffer_format == WL_SHM_FORMAT_XRGB8888) ?
        GBM_FORMAT_XRGB8888 : GBM_FORMAT_ARGB8888;

    auto buffer = toplevel->buffer;

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

    zwp_linux_buffer_params_v1_add_listener(buffer->params, &params_listener, toplevel);
    zwp_linux_buffer_params_v1_create(buffer->params, w, h, format, 0);
}

void WayfireChooserTopLevel::start_toplevel_source_ssession()
{
    copy_capture_source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(
        WayfireStreamChooserApp::getInstance().toplevel_capture_manager,
        handle);
    recording_session = ext_image_copy_capture_manager_v1_create_session(
        WayfireStreamChooserApp::getInstance().manager,
        copy_capture_source,
        0);
    ext_image_copy_capture_session_v1_add_listener(recording_session, &recording_session_listener, this);
}

void WayfireChooserTopLevel::frame_request()
{
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

void WayfireChooserTopLevel::buffer_ready()
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

    screenshot.set_paintable(texture);

    gbm_bo_unmap(buffer->bo, map_data);
}

/* Gtk Overlay showing information about a window */
WayfireChooserTopLevel::WayfireChooserTopLevel(ext_foreign_toplevel_handle_v1 *handle) : handle(handle)
{
    set_size_request(150, 150);
    set_valign(Gtk::Align::FILL);
    set_halign(Gtk::Align::FILL);
    layout = std::make_shared<ToplevelLayout>();
    set_layout_manager(layout);
    append(overlay);
    append(label);
    overlay.set_child(screenshot);
    overlay.add_overlay(icon);
    icon.set_halign(Gtk::Align::START);
    icon.set_valign(Gtk::Align::END);
    screenshot.set_halign(Gtk::Align::FILL);
    screenshot.set_valign(Gtk::Align::FILL);
    label.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    label.set_max_width_chars(40);

    buffer = std::make_shared<toplevel_buffer>();

    start_toplevel_source_ssession();

    ext_foreign_toplevel_handle_v1_add_listener(handle, &listener, this);
}

void WayfireChooserTopLevel::set_title(std::string title)
{
    buffered_title = title;
}

void WayfireChooserTopLevel::set_app_id(std::string app_id)
{
    buffered_app_id = app_id;
}

void WayfireChooserTopLevel::set_identifier(std::string identifier)
{
    buffered_identifier = identifier;
}

void WayfireChooserTopLevel::commit()
{
    if (buffered_app_id != "")
    {
        app_id = buffered_app_id;
        IconProvider::image_set_icon(icon, app_id);
        buffered_app_id = "";
    }

    if (buffered_title != "")
    {
        title = buffered_title;
        label.set_label(title);
        buffered_title = "";
    }

    if (buffered_identifier != "")
    {
        identifier = buffered_identifier;
        buffered_identifier = "";
    }
}

WayfireChooserTopLevel::~WayfireChooserTopLevel()
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
}

void WayfireChooserTopLevel::print()
{
    std::cout << "Window: " << identifier << std::endl;
    exit(0);
}
