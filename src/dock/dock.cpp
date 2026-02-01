#include <gtkmm/window.h>
#include <gdkmm/frameclock.h>
#include <glibmm/main.h>
#include <gdk/wayland/gdkwayland.h>

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
        window->set_auto_exclusive_zone(false);
        gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);
        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, 0);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, 0);
        out_box.append(box);
        out_box.add_css_class("out-box");
        box.add_css_class("box");
        window->set_child(out_box);

        window->add_css_class("wf-dock");

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
        _wl_surface = gdk_wayland_surface_get_wl_surface(
            window->get_surface()->gobj());

        box.add_tick_callback([=] (Glib::RefPtr<Gdk::FrameClock> fc)
        {
            set_clickable_region();
            return true;
        });
    }

    void add_child(Gtk::Widget& widget)
    {
        box.append(widget);
    }

    void rem_child(Gtk::Widget& widget)
    {
        this->box.remove(widget);
    }

    wl_surface *get_wl_surface()
    {
        return this->_wl_surface;
    }

    /* Sets the central section as clickable and transparent edges as click-through
     *  Gets called regularly to ensure css size changes all register */
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
