#include <gtkmm.h>
#include <glibmm.h>
#include <gdk/wayland/gdkwayland.h>
#include <gtk4-layer-shell.h>

#include "dock.hpp"
#include "wf-shell-app.hpp"
#include "wf-autohide-window.hpp"
#include "../util/gtk-utils.hpp"

class WfDock::impl
{
    WayfireOutput *output;
    std::unique_ptr<WayfireAutohidingWindow> window;
    wl_surface *_wl_surface;
    Gtk::Box out_box;
    Gtk::Box box;

    WfOption<std::string> css_path{"dock/css_path"};
    WfOption<std::string> position{"dock/position"};

  public:
    impl(WayfireOutput *output)
    {
        this->output = output;
        window = std::unique_ptr<WayfireAutohidingWindow>(
            new WayfireAutohidingWindow(output, "dock"));
        window->set_auto_exclusive_zone(false);
        gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);

        out_box.append(box);
        out_box.add_css_class("out_box");

        box.add_css_class("box");

        out_box.set_halign(Gtk::Align::CENTER);
        window->add_css_class("wf-dock");
        window->set_child(out_box);

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

        auto update_position = [=] ()
        {
            if (position.value() == "bottom")
            {
                // this is not great, but we lack better options without doing a
                // layout with boxes in boxes (ugly) or some sort of custom layout manager
                box.set_orientation(Gtk::Orientation::HORIZONTAL);
                box.set_direction(Gtk::TextDirection::LTR);
            } else if (position.value() == "left")
            {
                box.set_orientation(Gtk::Orientation::VERTICAL);
                box.set_direction(Gtk::TextDirection::LTR);
            } else if (position.value() == "right")
            {
                box.set_orientation(Gtk::Orientation::VERTICAL);
                box.set_direction(Gtk::TextDirection::RTL);
            } else // top
            {
                box.set_orientation(Gtk::Orientation::HORIZONTAL);
                box.set_direction(Gtk::TextDirection::LTR);
            }
        };
        position.set_callback(update_position);
        update_position();
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
     * Gets called regularly to ensure css size changes all register */
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
