#include <iostream>

#include <sys/mman.h>
#include <gdk/wayland/gdkwayland.h>
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "glib.h"
#include "glibmm/main.h"
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
    printf("%s\n", __func__);
}

static void toplevel_handle_done(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->commit();
    printf("%s\n", __func__);
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

/* SHM Callbacks*/
static int backingfile(off_t size)
{
    static int count;
    char name[128];
    sprintf(name, "/tmp/wf-stream-chooser-%d-XXXXXX", count++);
    int fd = mkstemp(name);
    if (fd < 0)
    {
        perror("mkstemp");
        return -1;
    }

    int ret;
    while ((ret = ftruncate(fd, size)) == EINTR)
    {}

    if (ret < 0)
    {
        perror("ret < 0");
        close(fd);
        return -1;
    }

    unlink(name);
    return fd;
}

static struct wl_buffer *create_shm_buffer(int width, int height, void **data_out, size_t *size)
{
    *size = width * 4 * height;
    printf("size: %ld\n", *size);

    int fd = backingfile(*size);
    if (fd < 0)
    {
        perror("Creating a buffer file failed");
        return NULL;
    }

    void *data = mmap(NULL, *size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        perror("mmap failed");
        close(fd);
        return NULL;
    }

    wl_shm_pool *pool = wl_shm_create_pool(WayfireStreamChooserApp::getInstance().shm, fd, *size);
    close(fd);
    wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
        width * 4, WL_SHM_FORMAT_ABGR8888);
    wl_shm_pool_destroy(pool);

    *data_out = data;
    return buffer;
}

void free_shm_buffer(std::shared_ptr<toplevel_buffer>& buffer)
{
    if (buffer->buffer == NULL)
    {
        return;
    }

    munmap(buffer->data, buffer->size);
    wl_buffer_destroy(buffer->buffer);
    buffer->buffer = NULL;
}

static void session_handle_buffer_size(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t width, uint32_t height)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;

    printf("%s : %d %d\n", __func__, width, height);

    toplevel->current_buffer_width  = width;
    toplevel->current_buffer_height = height;
}

static void session_handle_shm_format(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t format)
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;

    printf("%s : %d\n", __func__, format);

    toplevel->current_buffer_format = format;
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
{
    WayfireChooserTopLevel *toplevel = (WayfireChooserTopLevel*)data;
    toplevel->size();

    printf("%s\n", __func__);
}

static void session_handle_stopped(void*,
    struct ext_image_copy_capture_session_v1 *session)
{
    printf("%s\n", __func__);
    ext_image_copy_capture_session_v1_destroy(session);
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
}

static void frame_handle_failed(void*,
    struct ext_image_copy_capture_frame_v1 *handle,
    uint32_t reason)
{
    std::cerr << "Failed to copy frame because reason: " << reason << std::endl;
    ext_image_copy_capture_frame_v1_destroy(handle);
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener = {
    .transform = frame_handle_transform,
    .damage    = frame_handle_damage,
    .presentation_time = frame_handle_presentation_time,
    .ready  = frame_handle_ready,
    .failed = frame_handle_failed,
};

void WayfireChooserTopLevel::grab_toplevel_screenshot()
{
    printf("%s: %p : %p\n", __func__, handle,
        WayfireStreamChooserApp::getInstance().toplevel_capture_manager);
    auto copy_capture_source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(
        WayfireStreamChooserApp::getInstance().toplevel_capture_manager, handle);
    recording_session = ext_image_copy_capture_manager_v1_create_session(
        WayfireStreamChooserApp::getInstance().manager, copy_capture_source,
        EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS);
    ext_image_copy_capture_session_v1_add_listener(recording_session, &recording_session_listener, this);
}

void WayfireChooserTopLevel::size()
{
    if ((current_buffer_width <= 0) || (current_buffer_height <= 0))
    {
        printf("%s invalid size\n", __func__);
        return;
    }

    buffer = std::make_shared<toplevel_buffer>();

    if (frame)
    {
        ext_image_copy_capture_frame_v1_destroy(frame);
    }

    buffer->width  = current_buffer_width;
    buffer->height = current_buffer_height;

    frame = ext_image_copy_capture_session_v1_create_frame(recording_session);
    buffer->frame = frame;
    ext_image_copy_capture_frame_v1_add_listener(buffer->frame, &frame_listener, this);

    free_shm_buffer(buffer);
    buffer->buffer =
        create_shm_buffer(buffer->width, buffer->height, &buffer->data, &buffer->size);

    if (buffer->buffer == NULL)
    {
        printf("%s failed to create buffer\n", __func__);
        exit(EXIT_FAILURE);
    }

    ext_image_copy_capture_frame_v1_attach_buffer(buffer->frame, buffer->buffer);
    ext_image_copy_capture_frame_v1_damage_buffer(buffer->frame, 0, 0, buffer->width, buffer->height);
    ext_image_copy_capture_frame_v1_capture(buffer->frame);
}

void WayfireChooserTopLevel::buffer_ready()
{
    if ((buffer == nullptr) || (buffer->buffer == nullptr))
    {
        printf("%s buffer null\n", __func__);

        return;
    }

    /* buffer->data is now valid */
    std::shared_ptr<Glib::Bytes> bytes = 0;
    size_t size = buffer->size;
    if (buffer->data)
    {
        bytes = Glib::Bytes::create(buffer->data, size);
    }

    if (!bytes)
    {
        return;
    }

    auto builder = Gdk::MemoryTextureBuilder::create();
    builder->set_bytes(bytes);
    builder->set_width(buffer->width);
    builder->set_height(buffer->height);
    builder->set_stride(buffer->width * 4);
    builder->set_format(Gdk::MemoryFormat::R8G8B8A8);

    auto texture = builder->build();

    screenshot.set_paintable(texture);
    ext_image_copy_capture_frame_v1_destroy(frame);
    ext_image_copy_capture_session_v1_destroy(recording_session);
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
        icon.set_from_icon_name(app_id);
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

    /* If we have the protocols, grab a screenshot */
    if (WayfireStreamChooserApp::getInstance().has_image_copy_capture)
    {
        grab_toplevel_screenshot();
    }
}

WayfireChooserTopLevel::~WayfireChooserTopLevel()
{}

void WayfireChooserTopLevel::print()
{
    std::cout << "Window: " << identifier << std::endl;
    exit(0);
}
