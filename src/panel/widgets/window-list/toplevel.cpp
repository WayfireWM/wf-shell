#include <iostream>
#include <gtkmm.h>
#include <giomm/desktopappinfo.h>

#include <gdkmm/seat.h>
#include <gdk/wayland/gdkwayland.h>
#include <cmath>

#include <glibmm.h>
#include <cassert>
#include <sys/mman.h>

#include "toplevel.hpp"
#include "window-list.hpp"
#include "gtk-utils.hpp"
#include "panel.hpp"

namespace
{
extern zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl;
}

namespace IconProvider
{
void set_image_from_icon(Gtk::Image& image,
    std::string app_id_list, int size, int scale);
}

static int create_anon_file(off_t size)
{
    int fd = memfd_create("wf-live-preview", MFD_CLOEXEC);

    if (fd == -1)
    {
        perror("memfd_create");
        return 1;
    }

    if (ftruncate(fd, size) == -1)
    {
        perror("ftruncate");
        close(fd);
        return 1;
    }

    return fd;
}

void handle_frame_buffer(void *data,
    struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
    uint32_t format,
    uint32_t width,
    uint32_t height,
    uint32_t stride)
{
    TooltipMedia *tooltip_media = (TooltipMedia*)data;

    size_t size = width * height * int(stride / width);

    if (tooltip_media->size != size)
    {
        tooltip_media->set_size_request(width, height);
        tooltip_media->size = size;
        auto anon_file = create_anon_file(size);
        if (anon_file < 0)
        {
            perror("anon_file < 0");
            return;
        }

        void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, anon_file, 0);
        if (data == MAP_FAILED)
        {
            perror("data == MAP_FAILED");
            close(anon_file);
            return;
        }

        wl_shm_pool *pool = wl_shm_create_pool(tooltip_media->window_list->shm, anon_file, size);
        tooltip_media->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
        wl_shm_pool_destroy(pool);
        close(anon_file);
        tooltip_media->buffer_width  = width;
        tooltip_media->buffer_height = height;
        tooltip_media->buffer_stride = stride;
        tooltip_media->shm_data = data;
    }

    zwlr_screencopy_frame_v1_copy(tooltip_media->frame, tooltip_media->buffer);
}

void handle_frame_flags(void*,
    struct zwlr_screencopy_frame_v1*,
    uint32_t)
{}

void handle_frame_ready(void *data,
    struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nsec)
{
    TooltipMedia *tooltip_media = (TooltipMedia*)data;

    auto bytes = Glib::Bytes::create(tooltip_media->shm_data, tooltip_media->size);

    auto builder = Gdk::MemoryTextureBuilder::create();
    builder->set_bytes(bytes);
    builder->set_width(tooltip_media->buffer_width);
    builder->set_height(tooltip_media->buffer_height);
    builder->set_stride(tooltip_media->buffer_stride);
    builder->set_format(Gdk::MemoryFormat::R8G8B8A8);

    auto texture = builder->build();

    tooltip_media->set_paintable(texture);
}

void handle_frame_failed(void*, struct zwlr_screencopy_frame_v1*)
{}

void handle_frame_damage(void*,
    struct zwlr_screencopy_frame_v1*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t)
{}

void handle_frame_linux_dmabuf(void*,
    struct zwlr_screencopy_frame_v1*,
    uint32_t,
    uint32_t,
    uint32_t)
{}

void handle_frame_buffer_done(void*, struct zwlr_screencopy_frame_v1*)
{}

static struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener =
{
    handle_frame_buffer,
    handle_frame_flags,
    handle_frame_ready,
    handle_frame_failed,
    handle_frame_damage,
    handle_frame_linux_dmabuf,
    handle_frame_buffer_done,
};

void TooltipMedia::request_next_frame()
{
    if (this->frame)
    {
        zwlr_screencopy_frame_v1_destroy(this->frame);
        this->frame = NULL;
    }

    if (!window_list->wayfire_window_list_output->output)
    {
        return;
    }

    this->frame = zwlr_screencopy_manager_v1_capture_output(window_list->screencopy_manager, 0,
        window_list->wayfire_window_list_output->output);
    zwlr_screencopy_frame_v1_add_listener(this->frame, &screencopy_frame_listener, this);
}

TooltipMedia::TooltipMedia(WayfireWindowList *window_list)
{
    this->window_list = window_list;
    this->shm = window_list->shm;
    this->screencopy_manager = window_list->screencopy_manager;

    this->add_tick_callback([=] (const Glib::RefPtr<Gdk::FrameClock>& clock)
    {
        return this->on_tick(clock);
    });
}

bool TooltipMedia::on_tick(const Glib::RefPtr<Gdk::FrameClock>& clock)
{
    this->request_next_frame();
    return G_SOURCE_CONTINUE;
}

class WayfireToplevel::impl
{
    zwlr_foreign_toplevel_handle_v1 *handle, *parent;
    std::vector<zwlr_foreign_toplevel_handle_v1*> children;
    uint32_t state;
    uint64_t view_id;

    Gtk::Button button;
    Gtk::Box custom_tooltip_content;
    TooltipMedia *tooltip_media;
    Glib::RefPtr<Gio::SimpleActionGroup> actions;

    Gtk::PopoverMenu popover;
    Glib::RefPtr<Gio::Menu> menu;
    Glib::RefPtr<Gio::MenuItem> minimize, maximize, close;
    Glib::RefPtr<Gio::SimpleAction> minimize_action, maximize_action, close_action;
    // Gtk::Box menu_box;
    Gtk::Box button_contents;
    Gtk::Image image;
    Gtk::Label label;
    // Gtk::PopoverMenu menu;
    Glib::RefPtr<Gtk::GestureDrag> drag_gesture;
    sigc::connection m_drag_timeout;
    std::vector<sigc::connection> signals;

    Glib::ustring app_id, title;

    WfOption<bool> middle_click_close{"panel/middle_click_close"};

  public:
    WayfireWindowList *window_list;

    impl(WayfireWindowList *window_list, zwlr_foreign_toplevel_handle_v1 *handle)
    {
        std::cout << "impl" << std::endl;
        window_list->ipc_client = WayfirePanelApp::get().get_ipc_server_instance()->create_client();

        if (!window_list->ipc_client)
        {
            std::cerr <<
                "Failed to connect to ipc. Live window previews will not be available. (are ipc and ipc-rules plugins loaded?)";
        }

        this->handle = handle;
        this->parent = nullptr;
        zwlr_foreign_toplevel_handle_v1_add_listener(handle,
            &toplevel_handle_v1_impl, this);

        button.add_css_class("window-button");
        button.add_css_class("flat");
        button.remove_css_class("activated");
        button_contents.append(image);
        button_contents.append(label);
        button_contents.set_halign(Gtk::Align::START);
        button_contents.set_hexpand(true);
        button_contents.set_spacing(5);
        button.set_child(button_contents);
        // button.set_tooltip_text("none");
        tooltip_media = Gtk::make_managed<TooltipMedia>(window_list);
        this->custom_tooltip_content.append(*tooltip_media);
        // button.set_hover_point(0, 0);
        // button.set_hover_timeout(0);
        button.signal_query_tooltip().connect([=] (int x, int y, bool keyboard_mode,
                                                   const Glib::RefPtr<Gtk::Tooltip>& tooltip)
        {
            return query_tooltip(x, y, keyboard_mode, tooltip);
        }, false);
        button.set_has_tooltip(true);

        label.set_ellipsize(Pango::EllipsizeMode::END);
        label.set_hexpand(true);

        button.property_scale_factor().signal_changed()
            .connect(sigc::mem_fun(*this, &WayfireToplevel::impl::on_scale_update));

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

        // Hey Kids, want to see a really stupid idea?
        // Button can only have one child! But setting the parent of a popover still works fine...
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
            popover.popup();
            long_press->set_state(Gtk::EventSequenceState::CLAIMED);
            click_gesture->set_state(Gtk::EventSequenceState::DENIED);
        }));
        click_gesture->set_button(0);
        signals.push_back(click_gesture->signal_pressed().connect(
            [=] (int count, double x, double y)
        {
            click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
        }));

        signals.push_back(click_gesture->signal_released().connect(
            [=] (int count, double x, double y)
        {
            int butt = click_gesture->get_current_button();
            if (butt == 1)
            {
                // Ah, it was a press after all!
                if (!ignore_next_click)
                {
                    this->on_clicked();
                }

                ignore_next_click = false;
            } else if ((butt == 2) && middle_click_close.value())
            {
                zwlr_foreign_toplevel_handle_v1_close(handle);
            } else if (butt == 3)
            {
                popover.popup();
            }
        }));
        auto motion_controller = Gtk::EventControllerMotion::create();
        motion_controller->signal_leave().connect([=] ()
        {
            wf::json_t live_window_release_output_request;
            live_window_release_output_request["method"] = "live_previews/release_output";
            this->window_list->ipc_client->send(live_window_release_output_request.serialize(),
                [=] (wf::json_t data)
            {
                if (data.serialize().find("error") != std::string::npos)
                {
                    std::cerr << data.serialize() << std::endl;
                    std::cerr << "Error releasing output for live preview stream!" << std::endl;
                }
            });
        });
        button.add_controller(long_press);
        button.add_controller(click_gesture);
        button.add_controller(motion_controller);

        this->window_list = window_list;

        send_rectangle_hints();
        set_state(0); // will set the appropriate button style
    }

    int grab_off_x;
    double grab_start_x, grab_start_y;
    double grab_abs_start_x;
    bool drag_exceeds_threshold;

    bool drag_paused()
    {
        std::cout << __func__ << std::endl;
        /*
         *  auto gseat = Gdk::Display::get_default()->get_default_seat()->get_wl_seat();
         *  //auto seat  = gdk_wayland_seat_get_wl_seat(gseat->gobj());
         *  zwlr_foreign_toplevel_handle_v1_activate(handle, gseat);
         */
        return false;
    }

    void on_drag_begin(double _x, double _y)
    {
        std::cout << __func__ << std::endl;
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
        std::cout << __func__ << std::endl;
        /* Window was not just clicked, but also dragged. Ignore the next click,
         * which is the one that happens when the drag gesture ends. */
        set_ignore_next_click();

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
        std::cout << __func__ << std::endl;
        int x     = _x + grab_start_x;
        int y     = _y + grab_start_y;
        int width = button.get_allocated_width();
        int height = button.get_allocated_height();

        window_list->set_top_widget(nullptr);
        set_classes(state);

        /* When a button is dropped after dnd, we ignore the unclick
         * event so action doesn't happen in addition to dropping.
         * If the drag ends and the unclick event happens outside
         * the button, unset ignore_next_click or else the next
         * click on the button won't cause action. */
        if ((x < 0) || (x > width) || (y < 0) || (y > height))
        {
            unset_ignore_next_click();
        }

        /* When dragging with touch or pen, we allow some small movement while
         * still counting the action as button press as opposed to only dragging. */
        if (!drag_exceeds_threshold)
        {
            unset_ignore_next_click();
            this->on_clicked();
        }

        drag_gesture->set_state(Gtk::EventSequenceState::DENIED);

        send_rectangle_hints();
    }

    void set_hide_text(bool hide_text)
    {
        std::cout << __func__ << std::endl;
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
        std::cout << __func__ << std::endl;
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
        std::cout << __func__ << std::endl;
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
        std::cout << __func__ << std::endl;
        zwlr_foreign_toplevel_handle_v1_close(handle);
    }

    bool ignore_next_click = false;
    void set_ignore_next_click()
    {
        std::cout << __func__ << std::endl;
        ignore_next_click = true;

        /* Make sure that the view doesn't show clicked on animations while
         * dragging (this happens only on some themes) */
        button.set_state_flags(Gtk::StateFlags::SELECTED |
            Gtk::StateFlags::DROP_ACTIVE | Gtk::StateFlags::PRELIGHT);
    }

    void unset_ignore_next_click()
    {
        std::cout << __func__ << std::endl;
        ignore_next_click = false;
        button.unset_state_flags(Gtk::StateFlags::SELECTED |
            Gtk::StateFlags::DROP_ACTIVE | Gtk::StateFlags::PRELIGHT);
    }

    void on_clicked()
    {
        std::cout << __func__ << std::endl;
        /* If the button was dragged, we don't want to register the click.
         * Subsequent clicks should be handled though. */
        if (ignore_next_click)
        {
            unset_ignore_next_click();
            return;
        }

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
        std::cout << __func__ << std::endl;
        set_app_id(app_id);
    }

    bool query_tooltip(int x, int y, bool keyboard_mode, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
    {
        std::cout << __func__ << std::endl;
        if (!window_list->wayfire_window_list_output->output)
        {
            return false;
        }

        if (this->popover.is_visible())
        {
            return false;
        }

        wf::json_t live_window_preview_stream_request;
        live_window_preview_stream_request["method"] = "live_previews/request_stream";
        wf::json_t view_id_int;
        view_id_int["id"] = this->view_id;
        live_window_preview_stream_request["data"] = view_id_int;
        this->window_list->ipc_client->send(live_window_preview_stream_request.serialize(),
            [=] (wf::json_t data)
        {
            if (data.serialize().find("error") != std::string::npos)
            {
                std::cerr << data.serialize() << std::endl;
                std::cerr << "Error acquiring live preview stream. (is live-previews wayfire plugin enabled?)" << std::endl;
            }
        });

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
        std::cout << __func__ << std::endl;
        WfOption<int> minimal_panel_height{"panel/minimal_height"};
        this->app_id = app_id;
        IconProvider::set_image_from_icon(image, app_id,
            std::min(int(minimal_panel_height), 24), button.get_scale_factor());
        this->view_id = get_view_id_from_full_app_id(app_id);
        if (this->view_id == 0)
        {
            std::cerr <<
                "Failed to get view id from app_id. (Is app_id_mode set to 'full' in wayfire workarounds?)" <<
                std::endl;
        }
    }

    void send_rectangle_hints()
    {
        std::cout << __func__ << std::endl;
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
        std::cout << __func__ << std::endl;
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
        std::cout << __func__ << std::endl;
        this->title = title;
        // button.set_tooltip_text(title);
        label.set_text(title);
    }

    uint32_t get_state()
    {
        std::cout << __func__ << std::endl;
        return this->state;
    }

    zwlr_foreign_toplevel_handle_v1 *get_parent()
    {
        std::cout << __func__ << std::endl;
        return this->parent;
    }

    void set_parent(zwlr_foreign_toplevel_handle_v1 *parent)
    {
        std::cout << __func__ << std::endl;
        this->parent = parent;
    }

    std::vector<zwlr_foreign_toplevel_handle_v1*>& get_children()
    {
        std::cout << __func__ << std::endl;
        return this->children;
    }

    void remove_button()
    {
        std::cout << __func__ << std::endl;
        window_list->remove(button);
        send_rectangle_hints();
    }

    void set_classes(uint32_t state)
    {
        std::cout << __func__ << std::endl;
        if (state & WF_TOPLEVEL_STATE_ACTIVATED)
        {
            button.add_css_class("activated");
            button.remove_css_class("flat");
        } else
        {
            button.add_css_class("flat");
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
        std::cout << __func__ << std::endl;
        this->state = state;
        set_classes(state);
    }

    ~impl()
    {
        std::cout << "~impl" << std::endl;
        gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
        if (m_drag_timeout)
        {
            m_drag_timeout.disconnect();
        }

        for (auto signal : signals)
        {
            signal.disconnect();
        }

        zwlr_foreign_toplevel_handle_v1_destroy(handle);
    }

    void handle_output_enter(wl_output *output)
    {
        std::cout << __func__ << std::endl;
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
        std::cout << __func__ << std::endl;
        if (window_list->output->wo == output)
        {
            window_list->remove(button);
            send_rectangle_hints();
        }
    }
};


WayfireToplevel::WayfireToplevel(WayfireWindowList *window_list,
    zwlr_foreign_toplevel_handle_v1 *handle) :
    pimpl(new WayfireToplevel::impl(window_list, handle))
{
    std::cout << "WayfireToplevel::WayfireToplevel" << std::endl;
}

std::vector<zwlr_foreign_toplevel_handle_v1*>& WayfireToplevel::get_children()
{
    std::cout << __func__ << std::endl;
    return pimpl->get_children();
}

uint32_t WayfireToplevel::get_state()
{
    std::cout << __func__ << std::endl;
    return pimpl->get_state();
}

void WayfireToplevel::send_rectangle_hint()
{
    std::cout << __func__ << std::endl;
    return pimpl->send_rectangle_hint();
}

WayfireToplevel::~WayfireToplevel()
{
    std::cout << "WayfireToplevel::~WayfireToplevel" << std::endl;
}

using toplevel_t = zwlr_foreign_toplevel_handle_v1*;
static void handle_toplevel_title(void *data, toplevel_t, const char *title)
{
    std::cout << __func__ << std::endl;
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->set_title(title);
}

static void handle_toplevel_app_id(void *data, toplevel_t, const char *app_id)
{
    std::cout << __func__ << std::endl;
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->set_app_id(app_id);
}

static void handle_toplevel_output_enter(void *data, toplevel_t, wl_output *output)
{
    std::cout << __func__ << std::endl;
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->handle_output_enter(output);
}

static void handle_toplevel_output_leave(void *data, toplevel_t, wl_output *output)
{
    std::cout << __func__ << std::endl;
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->handle_output_leave(output);
}

/* wl_array_for_each isn't supported in C++, so we have to manually
 * get the data from wl_array, see:
 *
 * https://gitlab.freedesktop.org/wayland/wayland/issues/34 */
template<class T>
static void array_for_each(wl_array *array, std::function<void(T)> func)
{
    std::cout << __func__ << std::endl;
    assert(array->size % sizeof(T) == 0); // do not use malformed arrays
    for (T *entry = (T*)array->data; (char*)entry < ((char*)array->data + array->size); entry++)
    {
        func(*entry);
    }
}

static void handle_toplevel_state(void *data, toplevel_t, wl_array *state)
{
    std::cout << __func__ << std::endl;
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
    std::cout << __func__ << std::endl;
// auto impl = static_cast<WayfireToplevel::impl*> (data);
}

static void remove_child_from_parent(WayfireToplevel::impl *impl, toplevel_t child)
{
    std::cout << __func__ << std::endl;
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
    std::cout << __func__ << std::endl;
    auto impl = static_cast<WayfireToplevel::impl*>(data);
    impl->remove_button();
    remove_child_from_parent(impl, handle);
    impl->window_list->handle_toplevel_closed(handle);
}

static void handle_toplevel_parent(void *data, toplevel_t handle, toplevel_t parent)
{
    std::cout << __func__ << std::endl;
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

/* Icon loading functions */
namespace IconProvider
{
using Icon = Glib::RefPtr<Gio::Icon>;

namespace
{
std::string tolower(std::string str)
{
    std::cout << __func__ << std::endl;
    for (auto& c : str)
    {
        c = std::tolower(c);
    }

    return str;
}
}

/* Gio::DesktopAppInfo
 *
 * Usually knowing the app_id, we can get a desktop app info from Gio
 * The filename is either the app_id + ".desktop" or lower_app_id + ".desktop" */
Icon get_from_desktop_app_info(std::string app_id)
{
    std::cout << __func__ << std::endl;
    Glib::RefPtr<Gio::DesktopAppInfo> app_info;

    std::vector<std::string> prefixes = {
        "",
        "org.kde.",
    };

    std::vector<std::string> app_id_variations = {
        app_id,
        tolower(app_id),
    };

    std::vector<std::string> suffixes = {
        "",
        ".desktop"
    };

    for (auto& prefix : prefixes)
    {
        for (auto& id : app_id_variations)
        {
            for (auto& suffix : suffixes)
            {
                if (!app_info)
                {
                    app_info = Gio::DesktopAppInfo::create(prefix + id + suffix);
                }
            }
        }
    }

    if (app_info) // success
    {
        return app_info->get_icon();
    }

    return Icon{};
}

/* Second method: Just look up the built-in icon theme,
 * perhaps some icon can be found there */

void set_image_from_icon(Gtk::Image& image,
    std::string app_id_list, int size, int scale)
{
    std::cout << __func__ << std::endl;
    std::string app_id;
    std::istringstream stream(app_id_list);

    /* Wayfire sends a list of app-id's in space separated format, other compositors
     * send a single app-id, but in any case this works fine */
    auto display = image.get_display();
    while (stream >> app_id)
    {
        auto icon = get_from_desktop_app_info(app_id);
        std::string icon_name = "unknown";
        if (!icon)
        {
            /* Perhaps no desktop app info, but we might still be able to
             * get an icon directly from the icon theme */
            if (Gtk::IconTheme::get_for_display(display)->lookup_icon(app_id, size))
            {
                icon_name = app_id;
            }
        } else
        {
            icon_name = icon->to_string();
        }

        image_set_icon(&image, icon_name);

        /* finally found some icon */
        if (icon_name != "unknown")
        {
            break;
        }
    }
}
}
