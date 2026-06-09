#include <iostream>
#include <gtkmm.h>

#include <gdkmm/seat.h>
#include <gdk/wayland/gdkwayland.h>
#include <cmath>

#include <glibmm.h>
#include <cassert>
#include <sys/mman.h>

#include "toplevel.hpp"
#include "window-list.hpp"
#include "gtk-utils.hpp"

namespace
{
extern zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl;
}

static void session_handle_buffer_size(void *data,
    struct ext_image_copy_capture_session_v1*,
    uint32_t width, uint32_t height)
{
    TooltipMedia *toplevel = (TooltipMedia*)data;
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
    TooltipMedia *toplevel = (TooltipMedia*)data;
    toplevel->current_buffer_format = format;
}

static void session_handle_done(void *data,
    struct ext_image_copy_capture_session_v1*)
{}

static void session_handle_stopped(void*,
    struct ext_image_copy_capture_session_v1 *session)
{}

static const struct ext_image_copy_capture_session_v1_listener recording_session_listener = {
    .buffer_size   = session_handle_buffer_size,
    .shm_format    = session_handle_shm_format,
    .dmabuf_device = session_handle_dmabuf_device,
    .dmabuf_format = session_handle_dmabuf_format,
    .done    = session_handle_done,
    .stopped = session_handle_stopped,
};

#ifdef HAVE_DMABUF
static void dmabuf_created(void *data, struct zwp_linux_buffer_params_v1*,
    struct wl_buffer *wl_buffer)
{
    TooltipMedia *tooltip_media = (TooltipMedia*)data;

    tooltip_media->buffer = wl_buffer;
}

static void dmabuf_failed(void *data, struct zwp_linux_buffer_params_v1*)
{
    TooltipMedia *tooltip_media = (TooltipMedia*)data;

    std::cerr << "Failed to create dmabuf, trying shm" << std::endl;
    tooltip_media->window_list->live_previews_dmabuf = false;
}

static const struct zwp_linux_buffer_params_v1_listener params_listener =
{
    .created = dmabuf_created,
    .failed  = dmabuf_failed,
};
#endif // HAVE_DMABUF

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
    TooltipMedia *tooltip_media = (TooltipMedia*)data;

    tooltip_media->frame_in_flight = false;

    if (tooltip_media->buffer == nullptr)
    {
        printf("%s buffer null\n", __func__);

        return;
    }

    uint32_t stride  = 0;
    void *map_data   = NULL;
    void *pixel_data = gbm_bo_map(tooltip_media->bo, 0, 0, tooltip_media->width, tooltip_media->height,
        GBM_BO_TRANSFER_READ, &stride, &map_data);
    if (!pixel_data)
    {
        perror("failed to map bo");
        return;
    }

    /* Scale */
    auto pixbuf = Gdk::Pixbuf::create_from_data(
        (const guint8*)pixel_data,
        Gdk::Colorspace::RGB,
        true,
        8,
        tooltip_media->width,
        tooltip_media->height,
        stride);

    auto w = 500;
    auto h = tooltip_media->height * (500.0f / tooltip_media->width);

    auto scaled_pixbuf = pixbuf->scale_simple(
        w, h, Gdk::InterpType::BILINEAR);

    /* Swap red and blue channels */
    size_t size = w * h * 4;
    pixel_data = scaled_pixbuf->get_pixels();
    std::shared_ptr<Glib::Bytes> bytes = Glib::Bytes::create((unsigned char*)pixel_data, size);

    if (!bytes)
    {
        gbm_bo_unmap(tooltip_media->bo, map_data);
        return;
    }

    auto builder = Gdk::MemoryTextureBuilder::create();
    builder->set_bytes(bytes);
    builder->set_width(w);
    builder->set_height(h);
    builder->set_stride(w * 4);
    builder->set_format(Gdk::MemoryFormat::B8G8R8A8);

    auto texture = builder->build();

    tooltip_media->set_paintable(texture);

    gbm_bo_unmap(tooltip_media->bo, map_data);
}

static void frame_handle_failed(void *data,
    struct ext_image_copy_capture_frame_v1 *handle,
    uint32_t reason)
{
    TooltipMedia *tooltip_media = (TooltipMedia*)data;
    std::cerr << "Failed to copy frame because reason: " << reason << std::endl;
    ext_image_copy_capture_frame_v1_destroy(handle);
    tooltip_media->frame = nullptr;
    tooltip_media->frame_in_flight = false;
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener = {
    .transform = frame_handle_transform,
    .damage    = frame_handle_damage,
    .presentation_time = frame_handle_presentation_time,
    .ready  = frame_handle_ready,
    .failed = frame_handle_failed,
};

static void frame_handle_linux_dmabuf(uint32_t width, uint32_t height, TooltipMedia *tooltip_media)
{
    auto format = (tooltip_media->current_buffer_format == WL_SHM_FORMAT_XRGB8888) ?
        GBM_FORMAT_XRGB8888 : GBM_FORMAT_ARGB8888;

    if (tooltip_media->bo)
    {
        gbm_bo_destroy(tooltip_media->bo);
        tooltip_media->bo = nullptr;
    }

    if (tooltip_media->params)
    {
        zwp_linux_buffer_params_v1_destroy(tooltip_media->params);
        tooltip_media->params = nullptr;
    }

    auto w = width;
    auto h = height;

    const uint64_t modifier = 0; // DRM_FORMAT_MOD_LINEAR
    tooltip_media->bo = gbm_bo_create_with_modifiers(tooltip_media->window_list->dmabuf_device, w, h,
        format, &modifier, 1);
    if (tooltip_media->bo == NULL)
    {
        tooltip_media->bo = gbm_bo_create(tooltip_media->window_list->dmabuf_device, w, h,
            format, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
    }

    if (tooltip_media->bo == NULL)
    {
        perror("failed to create gbm bo");
        return;
    }

    tooltip_media->width  = gbm_bo_get_width(tooltip_media->bo);
    tooltip_media->height = gbm_bo_get_height(tooltip_media->bo);
    tooltip_media->stride = gbm_bo_get_stride(tooltip_media->bo);
    tooltip_media->params = zwp_linux_dmabuf_v1_create_params(tooltip_media->window_list->dmabuf);

    uint64_t mod = gbm_bo_get_modifier(tooltip_media->bo);
    zwp_linux_buffer_params_v1_add(tooltip_media->params,
        gbm_bo_get_fd(tooltip_media->bo), 0,
        gbm_bo_get_offset(tooltip_media->bo, 0),
        gbm_bo_get_stride(tooltip_media->bo),
        mod >> 32, mod & 0xffffffff);

    zwp_linux_buffer_params_v1_add_listener(tooltip_media->params, &params_listener, tooltip_media);
    zwp_linux_buffer_params_v1_create(tooltip_media->params, w, h, format, 0);
}

void TooltipMedia::request_next_frame()
{
    if (this->frame)
    {
        ext_image_copy_capture_frame_v1_destroy(this->frame);
        this->frame = nullptr;
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

    bool dirty = (width != current_buffer_width) || (height != current_buffer_height) ||
        !buffer;

    width  = current_buffer_width;
    height = current_buffer_height;

    if (dirty)
    {
        frame_handle_linux_dmabuf(width, height, this);
    }

    if (!buffer)
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
    frame = frame;

    ext_image_copy_capture_frame_v1_add_listener(frame, &frame_listener, this);
    ext_image_copy_capture_frame_v1_attach_buffer(frame, buffer);
    ext_image_copy_capture_frame_v1_damage_buffer(frame, 0, 0, width, height);
    ext_image_copy_capture_frame_v1_capture(frame);
    frame_in_flight = true;
}

void TooltipMedia::start_toplevel_source_session()
{
    copy_capture_source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(
        this->window_list->toplevel_capture_manager,
        this->ext_handle);
    recording_session = ext_image_copy_capture_manager_v1_create_session(
        this->window_list->copy_capture_manager,
        copy_capture_source, 0);
    ext_image_copy_capture_session_v1_add_listener(recording_session, &recording_session_listener, this);
}

TooltipMedia::TooltipMedia(WayfireWindowList *window_list, ext_foreign_toplevel_handle_v1 *ext_handle)
{
    this->window_list = window_list;
    this->ext_handle  = ext_handle;
    this->shm = window_list->shm;
    set_can_shrink(true);
    set_content_fit(Gtk::ContentFit::CONTAIN);
    set_vexpand(false);
    set_hexpand(false);
    set_size_request(200, -1);

    start_toplevel_source_session();

    Glib::signal_timeout().connect(
        [this] ()
    {
        if (!timer_continue)
        {
            return false;
        }

        this->request_next_frame();

        return true;
    }, 33);
}

TooltipMedia::~TooltipMedia()
{
    timer_continue = false;

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

    if (
#ifdef HAVE_DMABUF
        !this->window_list->live_previews_dmabuf &&
#endif // HAVE_DMABUF
        this->shm_data && this->size)
    {
        if (munmap(this->shm_data, this->size) < 0)
        {
            perror("munmap failed");
        }
    }

    if (this->buffer)
    {
        wl_buffer_destroy(this->buffer);
    }

#ifdef HAVE_DMABUF
    if (this->params)
    {
        zwp_linux_buffer_params_v1_destroy(this->params);
    }

    if (this->bo && this->map_data)
    {
        gbm_bo_unmap(this->bo, this->map_data);
    }

    if (this->bo)
    {
        gbm_bo_destroy(this->bo);
    }

#endif // HAVE_DMABUF
}

class WayfireToplevel::impl
{
    zwlr_foreign_toplevel_handle_v1 *handle, *parent;
    ext_foreign_toplevel_handle_v1 *ext_handle;
    std::vector<zwlr_foreign_toplevel_handle_v1*> children;
    uint32_t state;
    uint64_t view_id;

    Gtk::Box custom_tooltip_content;
    TooltipMedia *tooltip_media;
    Glib::RefPtr<Gio::SimpleActionGroup> actions;

    Gtk::PopoverMenu popover;
    Glib::RefPtr<Gio::Menu> menu;
    Glib::RefPtr<Gio::MenuItem> minimize, maximize, close;
    Glib::RefPtr<Gio::SimpleAction> minimize_action, maximize_action, close_action;
    Gtk::Box button;
    Gtk::Image image;
    Gtk::Label label;
    Glib::RefPtr<Gtk::GestureDrag> drag_gesture;
    std::vector<sigc::connection> signals;
    sigc::connection button_leave_signal;

    Glib::ustring app_id, title;

    WfOption<bool> middle_click_close{"panel/middle_click_close"};

  public:
    WayfireWindowList *window_list;

    impl(WayfireWindowList *window_list, zwlr_foreign_toplevel_handle_v1 *handle)
    {
        this->window_list = window_list;
        this->handle = handle;
        this->parent = nullptr;
        zwlr_foreign_toplevel_handle_v1_add_listener(handle,
            &toplevel_handle_v1_impl, this);

        button.add_css_class("window-button");
        image.add_css_class("widget-icon");
        image.add_css_class("toplevel-icon");
        button.append(image);
        button.append(label);
        button.set_halign(Gtk::Align::FILL);
        button.set_hexpand(true);
        button.set_spacing(5);

        label.set_ellipsize(Pango::EllipsizeMode::END);
        label.set_hexpand(true);
        label.set_halign(Gtk::Align::START);

        signals.push_back(button.property_scale_factor().signal_changed()
            .connect(sigc::mem_fun(*this, &WayfireToplevel::impl::on_scale_update)));


        actions = Gio::SimpleActionGroup::create();

        close_action    = Gio::SimpleAction::create("close");
        minimize_action = Gio::SimpleAction::create_bool("minimize", false);
        maximize_action = Gio::SimpleAction::create_bool("maximize", false);
        signals.push_back(close_action->signal_activate().connect(sigc::mem_fun(*this,
            &WayfireToplevel::impl::on_menu_close)));
        signals.push_back(minimize_action->signal_change_state().connect(sigc::mem_fun(*this,
            &WayfireToplevel::impl::on_menu_minimize)));
        signals.push_back(maximize_action->signal_change_state().connect(sigc::mem_fun(*this,
            &WayfireToplevel::impl::on_menu_maximize)));

        actions->add_action(close_action);
        actions->add_action(minimize_action);
        actions->add_action(maximize_action);

        gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(button.gobj()));

        popover.insert_action_group("windowaction", actions);
        menu     = Gio::Menu::create();
        minimize = Gio::MenuItem::create("Minimize", "windowaction.minimize");
        maximize = Gio::MenuItem::create("Maximize", "windowaction.maximize");
        close    = Gio::MenuItem::create("Close", "windowaction.close");

        menu->append_item(minimize);
        menu->append_item(maximize);
        menu->append_item(close);
        popover.set_menu_model(menu);

        drag_gesture = Gtk::GestureDrag::create();
        signals.push_back(drag_gesture->signal_drag_begin().connect(
            sigc::mem_fun(*this, &WayfireToplevel::impl::on_drag_begin)));
        signals.push_back(drag_gesture->signal_drag_update().connect(
            sigc::mem_fun(*this, &WayfireToplevel::impl::on_drag_update)));
        signals.push_back(drag_gesture->signal_drag_end().connect(
            sigc::mem_fun(*this, &WayfireToplevel::impl::on_drag_end)));
        button.add_controller(drag_gesture);

        auto click_gesture = Gtk::GestureClick::create();
        auto long_press    = Gtk::GestureLongPress::create();
        long_press->set_touch_only(true);
        signals.push_back(long_press->signal_pressed().connect(
            [=] (double x, double y)
        {
            drag_exceeds_threshold = true; /* A lie, but fixes long touch again */
            long_press->set_state(Gtk::EventSequenceState::CLAIMED);
            click_gesture->set_state(Gtk::EventSequenceState::DENIED);
            drag_gesture->set_state(Gtk::EventSequenceState::DENIED);
            popover.popup();
        }));
        click_gesture->set_button(0);
        signals.push_back(click_gesture->signal_pressed().connect(
            [=] (int count, double x, double y)
        {}));

        signals.push_back(click_gesture->signal_released().connect(
            [=] (int count, double x, double y)
        {
            int butt = click_gesture->get_current_button();
            if ((butt == 2) && middle_click_close.value())
            {
                zwlr_foreign_toplevel_handle_v1_close(handle);
            } else if (butt == 3)
            {
                popover.popup();
            }
        }));
        button.add_controller(long_press);
        button.add_controller(click_gesture);

        auto motion_controller = Gtk::EventControllerMotion::create();
        button_leave_signal = motion_controller->signal_leave().connect([=] ()
        {
            unset_tooltip_media();
        });
        button.add_controller(motion_controller);
        motion_controller = Gtk::EventControllerMotion::create();
        signals.push_back(motion_controller->signal_enter().connect([=] (double x, double y)
        {
            set_tooltip_media();
        }));
        button.add_controller(motion_controller);
        button.set_tooltip_text("none");
        this->tooltip_media = nullptr;
        signals.push_back(button.signal_query_tooltip().connect([=] (int x, int y, bool keyboard_mode,
                                                                     const Glib::RefPtr<Gtk::Tooltip>
                                                                     & tooltip)
        {
            return query_tooltip(x, y, keyboard_mode, tooltip);
        }, false));
        button.set_has_tooltip(true);

        send_rectangle_hints();
        set_state(0); // will set the appropriate button style
    }

    void set_tooltip_media()
    {
        if (this->tooltip_media)
        {
            return;
        }

        this->tooltip_media = Gtk::make_managed<TooltipMedia>(this->window_list, this->ext_handle);
        this->custom_tooltip_content.append(*this->tooltip_media);
    }

    void unset_tooltip_media()
    {
        if (!this->tooltip_media)
        {
            return;
        }

        this->tooltip_media->unparent();
        this->tooltip_media = nullptr;
    }

    void set_list_toplevel_handle(ext_foreign_toplevel_handle_v1 *handle)
    {
        this->ext_handle = handle;
    }

    int grab_off_x;
    double grab_start_x, grab_start_y;
    double grab_abs_start_x;
    bool drag_exceeds_threshold;

    bool drag_paused()
    {
        return false;
    }

    void on_drag_begin(double _x, double _y)
    {
        // Set grab start, before transforming it to absolute position
        grab_start_x = _x;
        grab_start_y = _y;

        set_classes(state);
        window_list->set_top_widget(&button);

        // Find the distance between pointer X and button origin
        int x = window_list->get_absolute_position(_x, button);
        grab_abs_start_x = x;

        // Find button corner in window-relative coords
        int loc_x = window_list->get_absolute_position(0, button);
        grab_off_x = x - loc_x;

        drag_exceeds_threshold = false;
    }

    static constexpr int DRAG_THRESHOLD = 3;
    void on_drag_update(double _x, double y)
    {
        int x = _x + grab_start_x;
        x = window_list->get_absolute_position(x, button);
        if (std::abs(x - grab_abs_start_x) > DRAG_THRESHOLD)
        {
            drag_exceeds_threshold = true;
        }

        auto hovered_button = window_list->get_widget_at(x);
        Gtk::Widget *before = window_list->get_widget_before(x);

        if (hovered_button)
        {
            // Where are we in the button?
            auto allocation = hovered_button->get_allocation();
            int half_width  = allocation.get_width() / 2;
            int x_in_button = x - allocation.get_x();
            if (x_in_button < half_width) // Left Half
            {
                if (before == nullptr)
                {
                    gtk_box_reorder_child_after(window_list->gobj(), GTK_WIDGET(button.gobj()), nullptr);
                } else
                {
                    window_list->reorder_child_after(button, *before);
                }
            } else if (x_in_button > half_width) // Right Half
            {
                window_list->reorder_child_after(button, *hovered_button);
            }
        }

        /* Make sure the grabbed button always stays at the same relative position
         * to the DnD position */
        int target_x = x - grab_off_x;
        window_list->set_top_x(target_x);
    }

    void on_drag_end(double _x, double _y)
    {
        window_list->set_top_widget(nullptr);
        set_classes(state);

        if (!drag_exceeds_threshold)
        {
            this->on_clicked();
        }

        drag_gesture->set_state(Gtk::EventSequenceState::DENIED);

        send_rectangle_hints();
    }

    void set_hide_text(bool hide_text)
    {
        if (hide_text)
        {
            label.hide();
        } else
        {
            label.show();
        }
    }

    void on_menu_minimize(Glib::VariantBase vb)
    {
        bool val = g_variant_get_boolean(vb.gobj());
        send_rectangle_hint();
        if (!val)
        {
            zwlr_foreign_toplevel_handle_v1_unset_minimized(handle);
            return;
        }

        zwlr_foreign_toplevel_handle_v1_set_minimized(handle);
    }

    void on_menu_maximize(Glib::VariantBase vb)
    {
        bool val = g_variant_get_boolean(vb.gobj());
        if (!val)
        {
            zwlr_foreign_toplevel_handle_v1_unset_maximized(handle);
            return;
        }

        zwlr_foreign_toplevel_handle_v1_set_maximized(handle);
    }

    void on_menu_close(Glib::VariantBase vb)
    {
        zwlr_foreign_toplevel_handle_v1_close(handle);
    }

    void on_clicked()
    {
        bool child_activated = false;
        for (auto c : get_children())
        {
            if (window_list->toplevels[c]->get_state() & WF_TOPLEVEL_STATE_ACTIVATED)
            {
                child_activated = true;
                break;
            }
        }

        if (!(state & WF_TOPLEVEL_STATE_ACTIVATED) && !child_activated)
        {
            auto gseat = Gdk::Display::get_default()->get_default_seat();
            auto seat  = gdk_wayland_seat_get_wl_seat(gseat->gobj());
            zwlr_foreign_toplevel_handle_v1_activate(handle, seat);
        } else
        {
            send_rectangle_hint();
            if (state & WF_TOPLEVEL_STATE_MINIMIZED)
            {
                zwlr_foreign_toplevel_handle_v1_unset_minimized(handle);
            } else
            {
                zwlr_foreign_toplevel_handle_v1_set_minimized(handle);
            }
        }
    }

    void on_scale_update()
    {
        set_app_id(app_id);
    }

    bool query_tooltip(int x, int y, bool keyboard_mode, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
    {
        if (this->popover.is_visible())
        {
            return false;
        }

        if (!this->window_list->live_window_previews_enabled())
        {
            if (!this->window_list->normal_title_tooltips)
            {
                std::cerr <<
                    "Normal title tooltips enabled. To enable live window preview tooltips, make sure to set [panel] option 'live_window_previews = true' in wf-shell configuration and enable wayfire plugin live-previews"
                          <<
                    std::endl;
                this->window_list->enable_normal_tooltips_flag(true);
            }

            tooltip->set_text(title);
            return true;
        }

        this->window_list->live_window_preview_view_id = this->view_id;
        tooltip->set_custom(this->custom_tooltip_content);

        return true;
    }

    uint64_t get_view_id_from_full_app_id(const std::string& app_id)
    {
        const std::string sub_str = "wf-ipc-";
        size_t pos = app_id.find(sub_str);

        if (pos != std::string::npos)
        {
            size_t suffix_start_index = pos + sub_str.length();
            if (suffix_start_index < app_id.length())
            {
                try {
                    uint64_t view_id = std::stoi(app_id.substr(suffix_start_index, std::string::npos));
                    return view_id;
                } catch (...)
                {
                    return 0;
                }
            } else
            {
                return 0;
            }
        } else
        {
            return 0;
        }
    }

    void set_app_id(std::string app_id)
    {
        WfOption<int> minimal_panel_height{"panel/minimal_height"};
        this->app_id = app_id;
        IconProvider::image_set_icon(image, app_id);
        this->view_id = get_view_id_from_full_app_id(app_id);
        if (this->view_id == 0)
        {
            std::cerr << "Failed to get view id from app_id. " <<
                "(Ensure 'app_id_mode' set to 'full' in wayfire " <<
                "[workarounds] and restart wf-panel or the applications " <<
                "in the window list)" << std::endl;
        }
    }

    std::string get_app_id()
    {
        return this->app_id;
    }

    void send_rectangle_hints()
    {
        for (const auto& toplevel_button : window_list->toplevels)
        {
            if (toplevel_button.second && toplevel_button.second->pimpl)
            {
                toplevel_button.second->pimpl->send_rectangle_hint();
            }
        }
    }

    void send_rectangle_hint()
    {
        auto panel = WayfirePanelApp::get().panel_for_wl_output(window_list->output->wo);
        auto w     = button.get_width();
        auto h     = button.get_height();
        if (panel && (w > 0) && (h > 0))
        {
            double x, y;
            button.translate_coordinates(panel->get_window(), 0, 0, x, y);
            zwlr_foreign_toplevel_handle_v1_set_rectangle(handle, panel->get_wl_surface(),
                x, y, w, h);
        }
    }

    void set_title(std::string title)
    {
        this->title = title;
        if (!this->window_list->live_window_previews_enabled())
        {
            button.set_tooltip_text(title);
        }

        label.set_text(title);
    }

    uint32_t get_state()
    {
        return this->state;
    }

    zwlr_foreign_toplevel_handle_v1 *get_parent()
    {
        return this->parent;
    }

    void set_parent(zwlr_foreign_toplevel_handle_v1 *parent)
    {
        this->parent = parent;
    }

    std::vector<zwlr_foreign_toplevel_handle_v1*>& get_children()
    {
        return this->children;
    }

    void remove_button()
    {
        button_leave_signal.disconnect();
        auto children = window_list->get_children();
        if (std::count(children.begin(), children.end(), &button) == 1)
        {
            window_list->remove(button);
        }

        send_rectangle_hints();
    }

    void set_classes(uint32_t state)
    {
        if (state & WF_TOPLEVEL_STATE_ACTIVATED)
        {
            button.add_css_class("activated");
        } else
        {
            button.remove_css_class("activated");
        }

        if (state & WF_TOPLEVEL_STATE_MINIMIZED)
        {
            button.add_css_class("minimized");
            minimize_action->set_state(Glib::wrap(g_variant_new_boolean(true)));
        } else
        {
            button.remove_css_class("minimized");
            minimize_action->set_state(Glib::wrap(g_variant_new_boolean(false)));
        }

        if (state & WF_TOPLEVEL_STATE_MAXIMIZED)
        {
            button.add_css_class("maximized");
            maximize_action->set_state(Glib::wrap(g_variant_new_boolean(true)));
        } else
        {
            button.remove_css_class("maximized");
            maximize_action->set_state(Glib::wrap(g_variant_new_boolean(false)));
        }
    }

    void set_state(uint32_t state)
    {
        this->state = state;
        set_classes(state);
    }

    ~impl()
    {
        unset_tooltip_media();

        gtk_widget_unparent(GTK_WIDGET(popover.gobj()));

        button_leave_signal.disconnect();

        for (auto signal : signals)
        {
            signal.disconnect();
        }

        zwlr_foreign_toplevel_handle_v1_destroy(handle);
    }

    void handle_output_enter(wl_output *output)
    {
        if (this->parent)
        {
            return;
        }

        if (window_list->output->wo == output)
        {
            window_list->append(button);
            send_rectangle_hints();
        }
    }

    void handle_output_leave(wl_output *output)
    {
        if (window_list->output->wo == output)
        {
            auto children = window_list->get_children();
            if (std::count(children.begin(), children.end(), &button) == 1)
            {
                window_list->remove(button);
            }

            send_rectangle_hints();
        }
    }
};


WayfireToplevel::WayfireToplevel(WayfireWindowList *window_list,
    zwlr_foreign_toplevel_handle_v1 *handle) :
    pimpl(new WayfireToplevel::impl(window_list, handle))
{}

std::vector<zwlr_foreign_toplevel_handle_v1*>& WayfireToplevel::get_children()
{
    return pimpl->get_children();
}

uint32_t WayfireToplevel::get_state()
{
    return pimpl->get_state();
}

void WayfireToplevel::send_rectangle_hint()
{
    return pimpl->send_rectangle_hint();
}

WayfireToplevel::~WayfireToplevel()
{}

using toplevel_t = zwlr_foreign_toplevel_handle_v1*;
static void handle_toplevel_title(void *data, toplevel_t, const char *title)
{
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->set_title(title);
}

static void handle_toplevel_app_id(void *data, toplevel_t, const char *app_id)
{
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->set_app_id(app_id);
}

static void handle_toplevel_output_enter(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->handle_output_enter(output);
}

static void handle_toplevel_output_leave(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->handle_output_leave(output);
}

void WayfireToplevel::set_tooltip_media()
{
    pimpl->set_tooltip_media();
}

void WayfireToplevel::unset_tooltip_media()
{
    pimpl->unset_tooltip_media();
}

void WayfireToplevel::set_list_toplevel_handle(ext_foreign_toplevel_handle_v1 *handle)
{
    pimpl->set_list_toplevel_handle(handle);
}

/* wl_array_for_each isn't supported in C++, so we have to manually
 * get the data from wl_array, see:
 *
 * https://gitlab.freedesktop.org/wayland/wayland/issues/34 */
template<class T>
static void array_for_each(wl_array *array, std::function<void(T)> func)
{
    assert(array->size % sizeof(T) == 0); // do not use malformed arrays
    for (T *entry = (T*)array->data; (char*)entry < ((char*)array->data + array->size); entry++)
    {
        func(*entry);
    }
}

static void handle_toplevel_state(void *data, toplevel_t, wl_array *state)
{
    uint32_t flags = 0;
    array_for_each<uint32_t>(state, [&flags] (uint32_t st)
    {
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
        {
            flags |= WF_TOPLEVEL_STATE_ACTIVATED;
        }

        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
        {
            flags |= WF_TOPLEVEL_STATE_MAXIMIZED;
        }

        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
        {
            flags |= WF_TOPLEVEL_STATE_MINIMIZED;
        }
    });

    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->set_state(flags);
}

static void handle_toplevel_done(void *data, toplevel_t)
{
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    auto window_list = impl->window_list;

    auto wf_id = impl->get_view_id_from_full_app_id(impl->get_app_id());
    for (auto & list_toplevel : window_list->list_toplevels)
    {
        auto id = impl->get_view_id_from_full_app_id(list_toplevel.second->app_id);
        if (wf_id == id)
        {
            impl->set_list_toplevel_handle(list_toplevel.first);
            break;
        }
    }
}

static void remove_child_from_parent(WayfireToplevel::impl *impl, toplevel_t child)
{
    auto parent = impl->get_parent();
    auto& parent_toplevel = impl->window_list->toplevels[parent];
    if (child && parent && parent_toplevel)
    {
        auto& children = parent_toplevel->get_children();
        children.erase(std::find(children.begin(), children.end(), child));
    }
}

static void handle_toplevel_closed(void *data, toplevel_t handle)
{
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->remove_button();
    remove_child_from_parent(impl, handle);
    impl->window_list->handle_toplevel_closed(handle);
}

static void handle_toplevel_parent(void *data, toplevel_t handle, toplevel_t parent)
{
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    if (!parent)
    {
        if (impl->get_parent())
        {
            impl->handle_output_enter(impl->window_list->output->wo);
        }

        remove_child_from_parent(impl, handle);
        impl->set_parent(parent);
        return;
    }

    if (impl->window_list->toplevels[parent])
    {
        auto& children = impl->window_list->toplevels[parent]->get_children();
        children.push_back(handle);
    }

    impl->set_parent(parent);
    impl->remove_button();
}

namespace
{
struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl = {
    .title  = handle_toplevel_title,
    .app_id = handle_toplevel_app_id,
    .output_enter = handle_toplevel_output_enter,
    .output_leave = handle_toplevel_output_leave,
    .state  = handle_toplevel_state,
    .done   = handle_toplevel_done,
    .closed = handle_toplevel_closed,
    .parent = handle_toplevel_parent
};
}
