#include <iostream>
#include <glibmm.h>
#include <gdk/wayland/gdkwayland.h>
#include <giomm/desktopappinfo.h>
#include "group.hpp"


#include "window-list.hpp"

#include <fcntl.h>

static void handle_manager_toplevel(void *data, zwlr_foreign_toplevel_manager_v1 *manager,
    zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    window_list->handle_new_toplevel(toplevel);
}

static void handle_manager_finished(void *data, zwlr_foreign_toplevel_manager_v1 *manager)
{
    zwlr_foreign_toplevel_manager_v1_stop(manager);
    zwlr_foreign_toplevel_manager_v1_destroy(manager);
}

zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_v1_impl = {
    .toplevel = handle_manager_toplevel,
    .finished = handle_manager_finished,
};

/* Toplevel Callbacks */

static void toplevel_handle_closed(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    for (auto & toplevel : window_list->toplevels)
    {
        if (!toplevel.second)
        {
            continue;
        }

        if (toplevel.second->get_ext_handle() == handle)
        {
            toplevel.second->set_ext_handle(NULL);
            break;
        }
    }

    ext_foreign_toplevel_handle_v1_destroy(handle);
    window_list->list_toplevels.erase(handle);
}

static void toplevel_handle_done(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;

    for (auto & toplevel : window_list->toplevels)
    {
        if (!toplevel.second)
        {
            continue;
        }

        auto wf_id = window_list->get_view_id_from_full_app_id(toplevel.second->get_app_id());
        for (auto & list_toplevel : window_list->list_toplevels)
        {
            if (!list_toplevel.second)
            {
                continue;
            }

            auto id = window_list->get_view_id_from_full_app_id(list_toplevel.second->app_id);
            if (wf_id == id)
            {
                toplevel.second->set_ext_handle(list_toplevel.first);
                break;
            }
        }
    }
}

static void toplevel_handle_title(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle,
    const char *title)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    window_list->list_toplevels[handle]->title = title;
}

static void toplevel_handle_app_id(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle,
    const char *app_id)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    window_list->list_toplevels[handle]->app_id = app_id;
}

static void toplevel_handle_identifier(void *data,
    struct ext_foreign_toplevel_handle_v1 *handle,
    const char *identifier)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    window_list->list_toplevels[handle]->identifier = identifier;
}

ext_foreign_toplevel_handle_v1_listener toplevels_listener =
{
    .closed = toplevel_handle_closed,
    .done   = toplevel_handle_done,
    .title  = toplevel_handle_title,
    .app_id = toplevel_handle_app_id,
    .identifier = toplevel_handle_identifier,
};

/* Static callbacks for toplevel list object */
static void handle_toplevel(void *data,
    struct ext_foreign_toplevel_list_v1 *list,
    struct ext_foreign_toplevel_handle_v1 *handle)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    window_list->list_toplevels[handle] = std::make_unique<WayfireListToplevel>();
    ext_foreign_toplevel_handle_v1_add_listener(handle, &toplevels_listener, window_list);
}

static void handle_finished(void *data,
    struct ext_foreign_toplevel_list_v1 *list)
{
    ext_foreign_toplevel_list_v1_stop(list);
    ext_foreign_toplevel_list_v1_destroy(list);
}

ext_foreign_toplevel_list_v1_listener toplevel_list_v1_impl = {
    .toplevel = handle_toplevel,
    .finished = handle_finished,
};

static void dmabuf_feedback_done(void *data, struct zwp_linux_dmabuf_feedback_v1 *feedback)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;

    zwp_linux_dmabuf_feedback_v1_destroy(feedback);
    window_list->feedback = nullptr;
}

static void dmabuf_feedback_format_table(void*, struct zwp_linux_dmabuf_feedback_v1*,
    int32_t fd, uint32_t)
{
    close(fd);
}

static void dmabuf_feedback_main_device(void *data, struct zwp_linux_dmabuf_feedback_v1*,
    struct wl_array *device)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;

    int drm_fd;
    dev_t dev_id;
    std::string drm_device_name;
    memcpy(&dev_id, device->data, device->size);

    drmDevice *dev = NULL;
    if (drmGetDeviceFromDevId(dev_id, 0, &dev) != 0)
    {
        perror("Failed to get DRM device from dev id");
        return;
    }

    if (dev->available_nodes & (1 << DRM_NODE_RENDER))
    {
        drm_device_name = dev->nodes[DRM_NODE_RENDER];
    } else if (dev->available_nodes & (1 << DRM_NODE_PRIMARY))
    {
        drm_device_name = dev->nodes[DRM_NODE_PRIMARY];
    }

    drm_fd = open(drm_device_name.c_str(), O_RDWR);
    if (drm_fd < 0)
    {
        perror("Failed to open drm device");
        return;
    }

    window_list->dmabuf_device = gbm_create_device(drm_fd);
    if (window_list->dmabuf_device == NULL)
    {
        close(drm_fd);
        perror("Failed to create gbm device");
        return;
    }

    std::cout << "Live previews using drm device node: \"" << drm_device_name << "\"" << std::endl;

    drmFreeDevice(&dev);
    close(drm_fd);
}

static void dmabuf_feedback_tranche_done(void*, struct zwp_linux_dmabuf_feedback_v1*)
{}

static void dmabuf_feedback_tranche_target_device(void*, struct zwp_linux_dmabuf_feedback_v1*,
    struct wl_array*)
{}

static void dmabuf_feedback_tranche_formats(void*, struct zwp_linux_dmabuf_feedback_v1*,
    struct wl_array*)
{}

static void dmabuf_feedback_tranche_flags(void*, struct zwp_linux_dmabuf_feedback_v1*,
    uint32_t)
{}

static const struct zwp_linux_dmabuf_feedback_v1_listener dmabuf_feedback_listener = {
    .done = dmabuf_feedback_done,
    .format_table = dmabuf_feedback_format_table,
    .main_device  = dmabuf_feedback_main_device,
    .tranche_done = dmabuf_feedback_tranche_done,
    .tranche_target_device = dmabuf_feedback_tranche_target_device,
    .tranche_formats = dmabuf_feedback_tranche_formats,
    .tranche_flags   = dmabuf_feedback_tranche_flags,
};

static void registry_add_object(void *data, wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;

    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        auto zwlr_toplevel_manager = (zwlr_foreign_toplevel_manager_v1*)
            wl_registry_bind(registry, name,
            &zwlr_foreign_toplevel_manager_v1_interface,
            version);
        window_list->handle_toplevel_manager(zwlr_toplevel_manager);
        zwlr_foreign_toplevel_manager_v1_add_listener(window_list->manager,
            &toplevel_manager_v1_impl, window_list);
    } else if (strcmp(interface, ext_foreign_toplevel_list_v1_interface.name) == 0)
    {
        auto foreign_toplevel_list = (ext_foreign_toplevel_list_v1*)
            wl_registry_bind(registry, name,
            &ext_foreign_toplevel_list_v1_interface,
            version);
        window_list->foreign_toplevel_list = foreign_toplevel_list;
        ext_foreign_toplevel_list_v1_add_listener(foreign_toplevel_list,
            &toplevel_list_v1_impl, window_list);
    } else if (strcmp(interface, ext_image_copy_capture_manager_v1_interface.name) == 0)
    {
        auto copy_capture_manager = (ext_image_copy_capture_manager_v1*)wl_registry_bind(registry, name,
            &ext_image_copy_capture_manager_v1_interface, version);
        window_list->copy_capture_manager = copy_capture_manager;
    } else if (strcmp(interface, ext_foreign_toplevel_image_capture_source_manager_v1_interface.name) == 0)
    {
        auto toplevel_capture_manager =
            (ext_foreign_toplevel_image_capture_source_manager_v1*)wl_registry_bind(registry, name,
                &ext_foreign_toplevel_image_capture_source_manager_v1_interface, version);
        window_list->toplevel_capture_manager = toplevel_capture_manager;
    } else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0)
    {
        window_list->dmabuf = (zwp_linux_dmabuf_v1*)wl_registry_bind(registry, name,
            &zwp_linux_dmabuf_v1_interface, version);
        if (window_list->dmabuf)
        {
            window_list->feedback = zwp_linux_dmabuf_v1_get_default_feedback(window_list->dmabuf);
            zwp_linux_dmabuf_feedback_v1_add_listener(window_list->feedback, &dmabuf_feedback_listener,
                window_list);
        }
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

uint64_t WayfireWindowList::get_view_id_from_full_app_id(const std::string& app_id)
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

bool WayfireWindowList::is_vertical() const
{
    return (panel_position.value() == PANEL_POSITION_LEFT) ||
           (panel_position.value() == PANEL_POSITION_RIGHT);
}

void WayfireWindowList::update_layout()
{
    if (is_vertical())
    {
        scrolled_window.set_hexpand(false);
        scrolled_window.set_vexpand(true);

        scrolled_window.set_propagate_natural_width(false);
        scrolled_window.set_propagate_natural_height(true);

        scrolled_window.set_policy(
            Gtk::PolicyType::NEVER,
            Gtk::PolicyType::AUTOMATIC);
    } else
    {
        scrolled_window.set_hexpand(true);
        scrolled_window.set_vexpand(false);

        scrolled_window.set_propagate_natural_width(true);
        scrolled_window.set_propagate_natural_height(false);

        scrolled_window.set_policy(
            Gtk::PolicyType::AUTOMATIC,
            Gtk::PolicyType::NEVER);
    }

    for (auto& t : toplevels)
    {
        if (t.second)
        {
            t.second->set_hide_text(is_vertical() || hide_label.value());
        }
    }

    for (auto& g : groups)
    {
        if (g.second)
        {
            g.second->set_hide_text(is_vertical() || hide_label.value());
        }
    }

    queue_allocate();
}

void WayfireWindowList::handle_config_reload()
{
    update_layout();
    update_grouping();
}

void WayfireWindowList::init(Gtk::Box *container)
{
    auto gdk_display = gdk_display_get_default();
    auto display     = gdk_wayland_display_get_wl_display(gdk_display);

    this->display = display;

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    if (!this->manager)
    {
        std::cerr << "Compositor doesn't support" <<
            " wlr-foreign-toplevel-management." << std::endl;
        std::cerr << "The window-list widget will not be initialized." << std::endl;
        return;
    }

    if (!this->foreign_toplevel_list)
    {
        std::cerr << "Compositor doesn't support" <<
            " ext-foreign-toplevel-list-v1." << std::endl;
        std::cerr << "Live window previews cannot be enabled." << std::endl;
    }

    if (!this->toplevel_capture_manager)
    {
        std::cerr << "Compositor doesn't support" <<
            " ext-foreign-toplevel-image-copy-capture-v1." << std::endl;
        std::cerr << "Live window previews cannot be enabled." << std::endl;
    }

    scrolled_window.add_css_class("window-list");

    scrolled_window.set_child(*this);
    update_layout();
    container->append(scrolled_window);
}

void WayfireWindowList::set_top_widget(Gtk::Widget *top)
{
    this->layout->top_widget = top;

    if (layout->top_widget)
    {
        /* Start from the widget's current position so that it does not jump
         * before the drag position is updated. */
        this->layout->top_position = get_absolute_position(0, *top);
    }

    set_top_position(layout->top_position);
}

void WayfireWindowList::set_top_position(int position)
{
    if (this->layout->top_widget)
    {
        if (is_vertical())
        {
            position = std::min(position,
                get_allocated_height() -
                layout->top_widget->get_allocated_height());
        } else
        {
            position = std::min(position,
                get_allocated_width() -
                layout->top_widget->get_allocated_width());
        }

        position = std::max(position, 0);
    }

    this->layout->top_position = position;

    queue_allocate();
    queue_draw();
}

int WayfireWindowList::get_absolute_position(int position, Gtk::Widget& ref)
{
    auto w = &ref;
    while (w && w != this)
    {
        auto allocation = w->get_allocation();

        if (is_vertical())
        {
            position += allocation.get_y();
        } else
        {
            position += allocation.get_x();
        }

        w = w->get_parent();
    }

    return position;
}

Gtk::Widget*WayfireWindowList::get_widget_before(int position)
{
    Gtk::Allocation given_point;

    if (is_vertical())
    {
        given_point = Gtk::Allocation{
            get_allocated_width() / 2, position, 1, 1};
    } else
    {
        given_point = Gtk::Allocation{
            position, get_allocated_height() / 2, 1, 1};
    }

    /* Widgets are stored bottom to top, so we will return the bottom-most
     * widget at the given position */
    Gtk::Widget *previous = nullptr;
    auto children = this->get_children();
    for (auto& child : children)
    {
        if (layout->top_widget && (child == layout->top_widget))
        {
            continue;
        }

        if (child->get_allocation().intersects(given_point))
        {
            return previous;
        }

        previous = child;
    }

    return nullptr;
}

Gtk::Widget*WayfireWindowList::get_widget_at(int position)
{
    Gtk::Allocation given_point;

    if (is_vertical())
    {
        given_point = Gtk::Allocation{
            get_allocated_width() / 2, position, 1, 1};
    } else
    {
        given_point = Gtk::Allocation{
            position, get_allocated_height() / 2, 1, 1};
    }

    /* Widgets are stored bottom to top, so we will return the bottom-most
     * widget at the given position */
    auto children = this->get_children();
    for (auto& child : children)
    {
        if (child == layout->top_widget)
        {
            continue;
        }

        if (child->get_allocation().intersects(given_point))
        {
            return child;
        }
    }

    return nullptr;
}

void WayfireWindowList::handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
{
    this->manager = manager;
}

void WayfireWindowList::handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    toplevels[toplevel] = std::make_unique<WayfireToplevel>(this, toplevel);
    toplevels[toplevel]->set_hide_text(is_vertical() || hide_label.value());
}

void WayfireWindowList::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    auto it = toplevels.find(toplevel);
    if ((it != toplevels.end()) && it->second)
    {
        std::string norm_id = get_normalized_app_id(it->second->get_app_id());
        auto git = groups.find(norm_id);
        if ((git != groups.end()) && git->second)
        {
            git->second->remove_toplevel(it->second.get());
        }
    }

    toplevels.erase(toplevel);
    update_grouping();
}

WayfireWindowList::WayfireWindowList(WayfireOutput *output)
{
    this->output = output;

    layout = std::make_shared<WayfireWindowListLayout>(this);
    set_layout_manager(layout);
    user_size.set_callback([=]
    {
        this->queue_allocate();
    });
    enable_grouping.set_callback([=]
    {
        this->update_grouping();
    });
}

std::string WayfireWindowList::get_normalized_app_id(const std::string& app_id)
{
    std::string cleaned = app_id;
    const std::string sub_str = " wf-ipc-";
    size_t pos = cleaned.find(sub_str);
    if (pos != std::string::npos)
    {
        cleaned = cleaned.substr(0, pos);
    } else if (cleaned.rfind("wf-ipc-", 0) == 0)
    {
        return "";
    }

    std::istringstream stream(cleaned);
    std::string first_token;
    if (stream >> first_token)
    {
        return first_token;
    }

    return cleaned;
}

std::string WayfireWindowList::get_app_name(const std::string& normalized_app_id)
{
    if (normalized_app_id.empty())
    {
        return "Unknown";
    }

    std::string lower_id = normalized_app_id;
    for (auto& c : lower_id)
    {
        c = std::tolower(static_cast<unsigned char>(c));
    }

    std::vector<std::string> prefixes = {"", "org.kde."};
    std::vector<std::string> id_variations = {normalized_app_id, lower_id};
    std::string suffix = ".desktop";

    for (const auto& prefix : prefixes)
    {
        for (const auto& id : id_variations)
        {
            auto app_info = Gio::DesktopAppInfo::create(prefix + id + suffix);
            if (app_info)
            {
                auto name = app_info->get_display_name();
                if (name.empty())
                {
                    name = app_info->get_name();
                }

                if (!name.empty())
                {
                    return name;
                }
            }
        }
    }

    for (const auto& info : Gio::AppInfo::get_all())
    {
        auto desktop = std::dynamic_pointer_cast<Gio::DesktopAppInfo>(info);
        if (!desktop)
        {
            continue;
        }

        const auto wm_class = desktop->get_startup_wm_class();
        if (!wm_class.empty() && (wm_class == normalized_app_id))
        {
            auto name = desktop->get_display_name();
            if (name.empty())
            {
                name = desktop->get_name();
            }

            if (!name.empty())
            {
                return name;
            }
        }
    }

    return normalized_app_id;
}

bool WayfireWindowList::is_toplevel_grouped(WayfireToplevel *toplevel)
{
    if (!toplevel || !enable_grouping.value())
    {
        return false;
    }

    std::string norm_id = const_cast<WayfireWindowList*>(this)->get_normalized_app_id(toplevel->get_app_id());
    return !norm_id.empty() && (groups.count(norm_id) > 0);
}

void WayfireWindowList::update_grouping()
{
    std::map<std::string, std::vector<WayfireToplevel*>> by_app;

    if (enable_grouping.value())
    {
        for (auto& kv : toplevels)
        {
            auto *t = kv.second.get();
            if (!t || !t->is_visible_on_output())
            {
                continue;
            }

            std::string norm_id = get_normalized_app_id(t->get_app_id());
            if (!norm_id.empty())
            {
                by_app[norm_id].push_back(t);
            }
        }
    }

    /* Remove groups that no longer have >= 2 visible windows */
    for (auto it = groups.begin(); it != groups.end();)
    {
        auto app_it = by_app.find(it->first);
        if ((app_it == by_app.end()) || (app_it->second.size() < 2))
        {
            Gtk::Widget *prev_sibling = it->second->get_button().get_prev_sibling();
            if (app_it != by_app.end())
            {
                for (auto *t : app_it->second)
                {
                    t->set_grouped(false, prev_sibling);
                }
            }

            it = groups.erase(it);
        } else
        {
            ++it;
        }
    }

    /* Create or update groups */
    for (auto& kv : by_app)
    {
        const auto& norm_id = kv.first;
        auto& members = kv.second;

        if (members.size() < 2)
        {
            continue;
        }

        if (groups.find(norm_id) == groups.end())
        {
            Gtk::Widget *anchor_prev = nullptr;
            for (auto *t : members)
            {
                if (t->get_button().get_parent() == this)
                {
                    anchor_prev = t->get_button().get_prev_sibling();
                    break;
                }
            }

            auto group = std::make_unique<WayfireToplevelGroup>(this, norm_id);
            if (anchor_prev && (anchor_prev->get_parent() == this))
            {
                insert_child_after(group->get_button(), *anchor_prev);
            } else
            {
                append(group->get_button());
            }

            groups[norm_id] = std::move(group);
        }

        auto *group = groups[norm_id].get();
        for (auto *t : members)
        {
            group->add_toplevel(t);
            t->set_grouped(true);
        }

        group->sync();
    }

    /* Reinstate remaining toplevels */
    for (auto& kv : toplevels)
    {
        auto *t = kv.second.get();
        if (!t)
        {
            continue;
        }

        std::string norm_id = get_normalized_app_id(t->get_app_id());
        bool in_group = enable_grouping.value() && !norm_id.empty() && (groups.count(norm_id) > 0);
        if (!in_group)
        {
            t->set_grouped(false);
        }
    }

    queue_allocate();
}

WayfireWindowList::~WayfireWindowList()
{
    /* Call the toplevels destructors first.
     * This fixes a crash when a dmabuf tooltip is present
     * when the window-list widget is unloaded. */
    groups.clear();
    toplevels.clear();

    for (auto & list_toplevel : list_toplevels)
    {
        ext_foreign_toplevel_handle_v1_destroy(list_toplevel.first);
    }

    list_toplevels.clear();

    wl_registry_destroy(registry);

    if (this->manager)
    {
        zwlr_foreign_toplevel_manager_v1_stop(this->manager);
        zwlr_foreign_toplevel_manager_v1_destroy(this->manager);
    }

    if (this->foreign_toplevel_list)
    {
        ext_foreign_toplevel_list_v1_stop(this->foreign_toplevel_list);
        ext_foreign_toplevel_list_v1_destroy(this->foreign_toplevel_list);
    }

    if (this->copy_capture_manager)
    {
        ext_image_copy_capture_manager_v1_destroy(this->copy_capture_manager);
    }

    if (this->toplevel_capture_manager)
    {
        ext_foreign_toplevel_image_capture_source_manager_v1_destroy(this->toplevel_capture_manager);
    }

    if (this->dmabuf)
    {
        zwp_linux_dmabuf_v1_destroy(this->dmabuf);
    }

    if (this->feedback)
    {
        zwp_linux_dmabuf_feedback_v1_destroy(this->feedback);
    }

    if (this->dmabuf_device)
    {
        gbm_device_destroy(this->dmabuf_device);
    }
}
