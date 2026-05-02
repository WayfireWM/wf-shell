#include <iostream>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include <gdk/wayland/gdkwayland.h>
#include <ext-foreign-toplevel-list-v1-client-protocol.h>

#include "stream-chooser.hpp"
#include "gtkmm/enums.h"
#include "mainlayout.hpp"
#include "outputwidget.hpp"
#include "toplevelwidget.hpp"

/* Static callbacks for toplevel list object */
static void handle_toplevel(void *data,
    struct ext_foreign_toplevel_list_v1 *list,
    struct ext_foreign_toplevel_handle_v1 *toplevel)
{
    WayfireStreamChooserApp::getInstance().add_toplevel(toplevel);
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

/* Static callbacks for wayland registry */
static void registry_add_object(void *data, wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version)
{
    if (strcmp(interface, ext_foreign_toplevel_list_v1_interface.name) == 0)
    {
        auto list = (ext_foreign_toplevel_list_v1*)
            wl_registry_bind(registry, name,
            &ext_foreign_toplevel_list_v1_interface,
            version);
        WayfireStreamChooserApp::getInstance().has_foreign_toplevel_list = true;
        WayfireStreamChooserApp::getInstance().set_toplevel_list(list);
        ext_foreign_toplevel_list_v1_add_listener(list,
            &toplevel_list_v1_impl, NULL);
    } else if (strcmp(interface, ext_image_copy_capture_manager_v1_interface.name) == 0)
    {
        auto manager = (ext_image_copy_capture_manager_v1*)wl_registry_bind(registry, name,
            &ext_image_copy_capture_manager_v1_interface, version);
        WayfireStreamChooserApp::getInstance().has_image_copy_capture = true;
        WayfireStreamChooserApp::getInstance().set_copy_capture_manager(manager);
    } else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        auto shm = (wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, 1);
        WayfireStreamChooserApp::getInstance().set_shm(shm);
    } else if (strcmp(interface, ext_foreign_toplevel_image_capture_source_manager_v1_interface.name) == 0)
    {
        auto toplevel_capture_manager =
            (ext_foreign_toplevel_image_capture_source_manager_v1*)wl_registry_bind(registry, name,
                &ext_foreign_toplevel_image_capture_source_manager_v1_interface, version);
        WayfireStreamChooserApp::getInstance().has_image_capture_source = true;
        WayfireStreamChooserApp::getInstance().set_toplevel_capture_manager(toplevel_capture_manager);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

WayfireStreamChooserApp::WayfireStreamChooserApp() : Gtk::Application("org.wayfire.screen-chooser",
        Gio::Application::Flags::NONE)
{
    signal_activate().connect(sigc::mem_fun(*this, &WayfireStreamChooserApp::activate));
}

void WayfireStreamChooserApp::activate()
{
    window.add_css_class("stream-chooser");
    window.set_size_request(300, 300);
    add_window(window);
    window.set_child(main);
    layout = std::make_shared<MainLayout>();
    window.set_layout_manager(layout);
    main.add_css_class("main-chooser");
    main.set_valign(Gtk::Align::FILL);
    main.set_halign(Gtk::Align::FILL);
    main.append(header);
    main.append(notebook);
    main.append(buttons);
    auto window_display = window.get_display();
    auto css_provider   = Gtk::CssProvider::create();
    css_provider->load_from_data("window.stream-chooser { background-color: rgba(0, 0, 0, 0.5); }");
    Gtk::StyleContext::add_provider_for_display(window_display,
        css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);

    scroll_window.set_child(window_list);
    scroll_screen.set_child(screen_list);

    scroll_window.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll_screen.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);

    window_list.set_homogeneous(true);
    screen_list.set_homogeneous(true);

    window_list.set_halign(Gtk::Align::START);
    window_list.set_valign(Gtk::Align::START);
    screen_list.set_halign(Gtk::Align::START);
    screen_list.set_valign(Gtk::Align::START);

    notebook.set_expand(true);
    notebook.append_page(scroll_window, window_label);
    notebook.append_page(scroll_screen, screen_label);

    main.set_orientation(Gtk::Orientation::VERTICAL);

    buttons.set_hexpand(true);
    buttons.append(cancel);
    cancel.set_halign(Gtk::Align::START);
    buttons.append(done);
    done.set_halign(Gtk::Align::END);

    window_label.set_label("Window");
    screen_label.set_label("Screen");
    header.set_label("Choose a view to share");

    cancel.set_label("Cancel");
    done.set_label("Done");
    buttons.set_homogeneous(true);

    done.signal_clicked().connect([this] ()
    {
        if (notebook.get_current_page() == 0)
        {
            auto children = window_list.get_selected_children();
            if (children.size() == 1)
            {
                WayfireChooserTopLevel *cast_child = (WayfireChooserTopLevel*)(children[0]->get_child());
                cast_child->print();
            }

            /* TODO Consider an error to let user know the selection was invalid */
            exit(0);
        } else
        {
            auto children = screen_list.get_selected_children();
            if (children.size() == 1)
            {
                WayfireChooserOutput *cast_child = (WayfireChooserOutput*)(children[0]->get_child());
                cast_child->print();
            }

            /* TODO Consider an error to let user know the selection was invalid */
            exit(0);
        }
    });

    cancel.signal_clicked().connect([] ()
    {
        exit(0);
    });

    /* Attempt to get Window list */
    auto gdk_display = gdk_display_get_default();
    auto display     = gdk_wayland_display_get_wl_display(gdk_display);

    this->display = display;

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    this->registry = registry;
    wl_display_roundtrip(display);

    bool failed = false;
    if (!has_image_copy_capture)
    {
        failed = true;
        std::cerr << "Compositor has not advertised ext-image-copy-capture-v1" << std::endl;
    }

    if (!has_foreign_toplevel_list)
    {
        failed = true;
        std::cerr << "Compositor has not advertised ext-foreign-toplevel-list-v1" << std::endl;
    }

    if (!has_image_capture_source)
    {
        failed = true;
        std::cerr << "Compositor has not advertised ext-image-capture-source-v1" << std::endl;
    }

    if (failed)
    {
        window_label.set_sensitive(false);
        window_label.set_tooltip_text("This compositor does not currently support sharing individual windows");
        notebook.set_current_page(1);
    }

    /* Get output list */
    auto gtkdisplay = Gdk::Display::get_default();
    auto monitors   = gtkdisplay->get_monitors();
    monitors->signal_items_changed().connect(
        [this] (const int pos, const int rem, const int add)
    {
        auto display     = Gdk::Display::get_default();
        auto monitors    = display->get_monitors();
        int num_monitors = monitors->get_n_items();
        for (int i = 0; i < num_monitors; i++)
        {
            auto obj = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(i));
            add_output(obj);
        }
    });

    // Initial monitors
    int num_monitors = monitors->get_n_items();
    for (int i = 0; i < num_monitors; i++)
    {
        auto obj = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(i));
        add_output(obj);
    }

    auto debug = Glib::getenv("WF_CHOOSER_DEBUG");
    if (debug != "1")
    {
        gtk_layer_init_for_window(window.gobj());
        gtk_layer_set_namespace(window.gobj(), "chooser");
        gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
        gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
        gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
        gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);

        gtk_layer_set_keyboard_mode(window.gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        gtk_layer_set_exclusive_zone(window.gobj(), 0);
    }

    window.present();
}

void WayfireStreamChooserApp::add_toplevel(ext_foreign_toplevel_handle_v1 *handle)
{
    toplevels.emplace(handle, new WayfireChooserTopLevel(handle));
    window_list.append(*toplevels[handle]);
    if (window_list.get_selected_children().size() == 0)
    {
        auto child = window_list.get_child_at_index(0);
        window_list.select_child(*child);
    }
}

void WayfireStreamChooserApp::remove_toplevel(WayfireChooserTopLevel *toplevel)
{
    window_list.remove(*toplevel);
    toplevels.erase(toplevel->handle);
}

void WayfireStreamChooserApp::add_output(std::shared_ptr<Gdk::Monitor> monitor)
{
    std::string connector = monitor->get_connector();
    outputs.emplace(connector, new WayfireChooserOutput(monitor));
    screen_list.append(*outputs[connector]);
    if (screen_list.get_selected_children().size() == 0)
    {
        auto child = screen_list.get_child_at_index(0);
        screen_list.select_child(*child);
    }
}

void WayfireStreamChooserApp::set_shm(wl_shm *shm)
{
    this->shm = shm;
}

void WayfireStreamChooserApp::remove_output(std::string connector)
{
    screen_list.remove(*outputs[connector]);
    outputs.erase(connector);
}

void WayfireStreamChooserApp::set_toplevel_list(ext_foreign_toplevel_list_v1 *list)
{
    this->list = list;
}

void WayfireStreamChooserApp::set_copy_capture_manager(ext_image_copy_capture_manager_v1 *manager)
{
    this->manager = manager;
}

void WayfireStreamChooserApp::set_toplevel_capture_manager(
    ext_foreign_toplevel_image_capture_source_manager_v1 *toplevel_capture_manager)
{
    this->toplevel_capture_manager = toplevel_capture_manager;
}

/* Starting point */
int main(int argc, char **argv)
{
    WayfireStreamChooserApp::getInstance().run();
    exit(0);
}
