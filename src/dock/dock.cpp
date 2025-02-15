#include <gtkmm/window.h>
#include <glibmm/main.h>
#include <gdk/wayland/gdkwayland.h>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>
#include <gtk4-layer-shell.h>
#include <wf-autohide-window.hpp>

#include "dock.hpp"
#include "../util/gtk-utils.hpp"
#include <css-config.hpp>


class WfDock::impl
{
    WayfireOutput *output;
    std::unique_ptr<WayfireAutohidingWindow> window;
    wl_surface *_wl_surface;
    Gtk::Box out_box;
    Gtk::Box box;

    WfOption<std::string> css_path{"dock/css_path"};
    WfOption<int> dock_height{"dock/dock_height"};

  public:
    impl(WayfireOutput *output)
    {


        this->output = output;
        window = std::unique_ptr<WayfireAutohidingWindow>(
            new WayfireAutohidingWindow(output, "dock"));
        gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);
        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, 0);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, 0);
        out_box.append(box);
        out_box.get_style_context()->add_class("out-box");
        box.get_style_context()->add_class("box");
        window->set_child(out_box);

        window->set_interactive_debugging(true);
        window->get_style_context()->add_class("wf-dock");

        out_box.set_halign(Gtk::Align::CENTER);

        if ((std::string)css_path != "")
        {
            auto css = load_css_from_path(css_path);
            if (css)
            {
                auto display = Gdk::Display::get_default();
                Gtk::StyleContext::add_provider_for_display(display, css, GTK_STYLE_PROVIDER_PRIORITY_USER);
            }
        }
        window->present();
        new CssFromConfigInt("dock/icon_height", ".toplevel-icon {-gtk-icon-size:", "px;}");
        /*_wl_surface = gdk_wayland_window_get_wl_surface(
            window->gobj());*/
    }

    void add_child(Gtk::Widget& widget)
    {
        box.append(widget);
        set_clickable_region();
    }

    void rem_child(Gtk::Widget& widget)
    {
        this->box.remove(widget);
        set_clickable_region();
    }

    wl_surface *get_wl_surface()
    {
        return this->_wl_surface;
    }

    /* Sets the central section as clickable and transparent edges as click-through
       Should call it for every content change and output resize */
    void set_clickable_region()
    {
        auto surface = window->get_surface();
        auto widget_bounds = box.compute_bounds(*window);

        auto rect = Cairo::RectangleInt{
            (int)widget_bounds->get_x(),
            (int)widget_bounds->get_y(),
            (int)widget_bounds->get_width(),
            (int)widget_bounds->get_height()
        };

        auto region = Cairo::Region::create(rect);

        surface->set_input_region(region);
    }
};

WfDock::WfDock(WayfireOutput *output) :
    pimpl(new impl(output))
{}
WfDock::~WfDock() = default;

void WfDock::add_child(Gtk::Widget& w)
{
    return pimpl->add_child(w);
}

void WfDock::rem_child(Gtk::Widget& w)
{
    return pimpl->rem_child(w);
}

wl_surface*WfDock::get_wl_surface()
{
    return pimpl->get_wl_surface();
}
