#include <iostream>
#include <glibmm.h>
#include <gdk/gdkwayland.h>

#include "toplevel.hpp"
#include "window-list.hpp"
#include "panel.hpp"
#include "config.hpp"

#define DEFAULT_SIZE_PC 0.12

static void handle_manager_toplevel(void *data, zwlr_foreign_toplevel_manager_v1 *manager,
    zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    WayfireWindowList *window_list = (WayfireWindowList *) data;
    window_list->handle_new_toplevel(toplevel);
}

static void handle_manager_finished(void *data, zwlr_foreign_toplevel_manager_v1 *manager)
{
    /* TODO: maybe exit? */
}

zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_v1_impl = {
    .toplevel = handle_manager_toplevel,
    .finished = handle_manager_finished,
};

static void registry_add_object(void *data, wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    WayfireWindowList *window_list = (WayfireWindowList *) data;
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        auto zwlr_toplevel_manager = (zwlr_foreign_toplevel_manager_v1*)
            wl_registry_bind(registry, name,
                &zwlr_foreign_toplevel_manager_v1_interface,
                std::min(version, 1u));

        window_list->handle_toplevel_manager(zwlr_toplevel_manager);
    }
}
static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) { }

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

void WayfireWindowList::init(Gtk::HBox *container, wayfire_config *config)
{
    auto gdk_display = gdk_display_get_default();
    auto display = gdk_wayland_display_get_wl_display(gdk_display);

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    if (!this->manager)
    {
        std::cerr << "Compositor doesn't support" <<
            " wlr-foreign-toplevel-management, exiting." << std::endl;
        std::exit(-1);
    }

    wl_registry_destroy(registry);
    zwlr_foreign_toplevel_manager_v1_add_listener(manager,
        &toplevel_manager_v1_impl, this);

    scrolled_window.signal_draw().connect_notify(
        sigc::mem_fun(this, &WayfireWindowList::on_draw));

    scrolled_window.add(box);
    scrolled_window.set_propagate_natural_width(true);
    container->pack_start(scrolled_window, true, true);
    scrolled_window.show_all();

    /* Make sure new windows get added with the proper size */
    last_button_width = get_default_button_width();
}

void WayfireWindowList::set_button_width(int width)
{
    for (auto& toplevel : toplevels)
        toplevel.second->set_width(width);

    last_button_width = width;
}

int WayfireWindowList::get_default_button_width()
{
    int panel_width, panel_height;

    WayfirePanelApp::get().panel_for_wl_output(output->handle)
        ->get_window().get_size_request(panel_width, panel_height);

    return panel_width * DEFAULT_SIZE_PC;
}

void WayfireWindowList::on_draw(const Cairo::RefPtr<Cairo::Context>& ctx)
{
    int allocated_width = scrolled_window.get_allocated_width();
    std::pair<int32_t, int32_t> new_layout {allocated_width, toplevels.size()};

    int minimal_width, preferred_width;
    scrolled_window.get_preferred_width(minimal_width, preferred_width);

    /* We have changed the size/number of toplevels. On top of that, our list
     * is longer that the max size, so we need to re-layout the buttons */
    if (preferred_width > allocated_width && toplevels.size() > 0)
    {
        int new_width = allocated_width / toplevels.size();
        set_button_width(new_width);
    }
}

void WayfireWindowList::add_output(WayfireOutput *output)
{
    std::unique_ptr<WayfireWindowList>();
}

void WayfireWindowList::handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
{
    this->manager = manager;
}

void WayfireWindowList::handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle)
{
    toplevels[handle] = std::unique_ptr<WayfireToplevel> (new WayfireToplevel(this, handle, box));
    /* The size will be updated in the next on_draw() if needed */
    toplevels[handle]->set_width(last_button_width);
}

void WayfireWindowList::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
{
    toplevels.erase(handle);

    /* No size adjustments necessary in this case */
    if (toplevels.size() == 0)
        return;

    /* We can remove any special size requirements if the buttons all fit with
     * the default width, otherwise we still need to limit them to the allocation */
    int allowed_width = std::min(
        int(scrolled_window.get_allocated_width() / toplevels.size()),
        get_default_button_width());

    set_button_width(allowed_width);
}

WayfireWindowList::WayfireWindowList(WayfireOutput *output)
{
    this->output = output;
}

WayfireWindowList::~WayfireWindowList()
{
}
