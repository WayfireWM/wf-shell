#include <glibmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/application.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>

#include <stdio.h>
#include <iostream>
#include <sstream>

#include <map>

#include "widgets/battery.hpp"
#include "widgets/clock.hpp"
#include "widgets/launchers.hpp"
#include "widgets/network.hpp"
#include "widgets/spacing.hpp"

#include "wf-shell-app.hpp"

static void zwf_output_hide_panels(void *data,
                                   struct zwf_output_v1 *zwf_output_v1,
                                   uint32_t autohide)
{
    auto callback = (std::function<void(bool)>*) data;
    (*callback) (autohide);
}

const struct zwf_output_v1_listener zwf_output_impl =
{
    zwf_output_hide_panels
};

class WayfirePanel
{
    WayfireShellApp *app;
    Gtk::Window window;

    Gtk::HBox content_box;
    Gtk::HBox left_box, center_box, right_box;

    using Widget = std::unique_ptr<WayfireWidget>;
    using WidgetContainer = std::vector<Widget>;
    WidgetContainer left_widgets, center_widgets, right_widgets;

    WayfireOutput *output;
    zwf_wm_surface_v1 *wm_surface = NULL;

    int current_output_width = 0, current_output_height = 0;
    int hidden_height = 1;

    wf_option panel_position;
    const std::string panel_position_bottom = "bottom";

    int get_hidden_y()
    {
        return hidden_height - window.get_allocated_height();
    }

    wf_option duration = new_static_option("300");
    wf_duration transition{duration};

    bool animation_running = false;
    bool update_margin()
    {
        if (animation_running || transition.running())
        {
            int target_y = std::round(transition.progress());

            // takes effect only for anchored edges
            zwf_wm_surface_v1_set_margin(wm_surface,
                target_y, target_y, target_y, target_y);

            window.queue_draw();
            return (animation_running = transition.running());
        }

        return false;
    }

    void update_position()
    {
        uint32_t anchor = 0;
        if (panel_position->as_string() == "top")
            anchor = ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP;
        else
            anchor = ZWF_WM_SURFACE_V1_ANCHOR_EDGE_BOTTOM;

        zwf_wm_surface_v1_set_anchor(wm_surface, anchor);
    }

    bool show()
    {
        int start = transition.progress();
        transition.start(start, 0);
        update_margin();
        return false; // disconnect
    }

    bool hide()
    {
        int start = transition.progress();
        transition.start(start, get_hidden_y());
        update_margin();
        return false; // disconnect
    }

    sigc::connection pending_show, pending_hide;
    void schedule_show(int delay)
    {
        pending_hide.disconnect();
        if (!pending_show.connected())
            pending_show = Glib::signal_timeout().connect(sigc::mem_fun(this, &WayfirePanel::show), delay);
    }

    void schedule_hide(int delay)
    {
        pending_show.disconnect();
        if (!pending_hide.connected())
            pending_hide = Glib::signal_timeout().connect(sigc::mem_fun(this, &WayfirePanel::hide), delay);
    }

    void update_reserved_area()
    {
        if (!wm_surface)
            return;

        if (autohide_enabled()) {
            zwf_wm_surface_v1_set_exclusive_zone(wm_surface, 0);
        } else if (!autohide_enabled()) {
            zwf_wm_surface_v1_set_exclusive_zone(wm_surface, window.get_height());
        }
    }

    int autohide_count = 0;
    wf_option autohide_opt;
    wf_option_callback update_autohide = [=] ()
    {
        if (autohide_enabled() && !input_entered)
            schedule_hide(0);

        if (!autohide_enabled())
            schedule_show(0);

        update_reserved_area();
    };

    bool autohide_enabled()
    {
        return autohide_count + (bool)autohide_opt->as_int();
    }

    std::function<void(bool)> update_autohide_request = [=] (bool autohide)
    {
        autohide_count += autohide ? 1 : -1;
        update_autohide();
    };

    void create_wm_surface()
    {
        auto gdk_window = window.get_window()->gobj();
        auto surface = gdk_wayland_window_get_wl_surface(gdk_window);

        if (!surface)
        {
            std::cerr << "Error: created window was not a wayland surface" << std::endl;
            std::exit(-1);
        }

        wm_surface = zwf_output_v1_get_wm_surface(output->zwf, surface,
                                                  ZWF_OUTPUT_V1_WM_ROLE_OVERLAY);
        update_reserved_area();
        update_position();
        zwf_output_v1_add_listener(output->zwf, &zwf_output_impl, &update_autohide_request);
    }

    void handle_output_resize(uint32_t width, uint32_t height)
    {
        this->current_output_width = width;
        this->current_output_height = height;

        window.set_size_request(width, height * 0.05);
        window.show_all();

        if (!wm_surface)
            create_wm_surface();

        transition.start(get_hidden_y(), get_hidden_y());
        show();
        update_reserved_area();
        if (autohide_enabled())
            schedule_hide(1000);
    }

    void on_draw(const Cairo::RefPtr<Cairo::Context>& ctx)
    {
        update_margin();
    }

    int input_entered = 0;
    void on_enter(GdkEventCrossing *cross)
    {
        // ignore events between the window and widgets
        if (cross->detail != GDK_NOTIFY_NONLINEAR &&
            cross->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL)
            return;

        schedule_show(300); // TODO: maybe configurable?
        ++input_entered;
    }

    void on_leave(GdkEventCrossing *cross)
    {
        // ignore events between the window and widgets
        if (cross->detail != GDK_NOTIFY_NONLINEAR &&
            cross->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL)
            return;

        if (autohide_enabled())
            schedule_hide(500);
        --input_entered;
    }

    int old_allocated_height = 0;
    void on_resized(Gtk::Allocation& alloc)
    {
        if (old_allocated_height != alloc.get_height())
        {
            update_reserved_area();
            old_allocated_height = alloc.get_height();
        }
    }

    wf_option bg_color;
    void set_window_color()
    {
        if (bg_color->as_string() == "gtk_default")
            return window.unset_background_color();

        Gdk::RGBA rgba;
        if (bg_color->as_string() == "gtk_headerbar")
        {
            Gtk::HeaderBar headerbar;
            rgba = headerbar.get_style_context()->get_background_color();
        } else {
            auto color_string = bg_color->as_string();

            /* see if it is in #XXXXXX format */
            if (color_string.size() && color_string[0] == '$')
            {
                color_string[0] = '#';
                rgba.set(color_string);
            } else {
                /* otherwise, simply a list of double values, parse by default */
                auto color = bg_color->as_color();
                rgba.set_red(color.r);
                rgba.set_green(color.g);
                rgba.set_blue(color.b);
                rgba.set_alpha(color.a);
            }
        }

        window.override_background_color(rgba);
    }
    wf_option_callback background_callback;
    void setup_window()
    {
        window.set_resizable(false);
        window.set_decorated(false);

        bg_color = app->config->get_section("panel")
            ->get_option("background_color", "gtk_headerbar");

        background_callback = [=] () {
            set_window_color();
        };
        bg_color->add_updated_handler(&background_callback);
        set_window_color();

        window.signal_draw().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_draw));
        window.signal_enter_notify_event().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_enter));
        window.signal_leave_notify_event().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_leave));
        window.signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_resized));

        window.signal_focus_out_event().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_focus_out));

        window.signal_delete_event().connect(
            sigc::mem_fun(this, &WayfirePanel::on_delete));
    }

    bool on_delete(GdkEventAny *ev)
    {
        /* We ignore close events, because the panel's lifetime is bound to
         * the lifetime of the output */
        return true;
    }

    void on_focus_out(const GdkEventFocus *ev)
    {
        for (auto& w : left_widgets)
            w->focus_lost();
        for (auto& w : right_widgets)
            w->focus_lost();
        for (auto& w : center_widgets)
            w->focus_lost();

        /* We want to hide much faster, because this will have any effect
         * only in the case when there was an opened popup and the user wants
         * to hide the panel, so no use delaying it */
        if (autohide_enabled())
            schedule_hide(100);

        input_entered = 0; // also reset all inputs we might have missed
    }

    void init_layout()
    {
        content_box.pack_start(left_box, false, false);
        content_box.set_center_widget(center_box);
        content_box.pack_end(right_box, false, false);
        window.add(content_box);
    }

    Widget widget_from_name(std::string name)
    {
        if (name == "launchers")
            return Widget(new WayfireLaunchers());
        if (name == "clock")
            return Widget(new WayfireClock());
        if (name == "network")
            return Widget(new WayfireNetworkInfo());
        if (name == "battery")
            return Widget(new WayfireBatteryInfo());

        std::string spacing = "spacing";
        if (name.find(spacing) == 0)
        {
            auto pixel_str = name.substr(spacing.size());
            int pixel = std::atoi(pixel_str.c_str());

            if (pixel <= 0)
            {
                std::cerr << "Invalid spacing, " << pixel << std::endl;
                return nullptr;
            }

            return Widget(new WayfireSpacing(pixel));
        }

        std::cerr << "Invalid widget: " << name << std::endl;
        return nullptr;
    }

    static std::vector<std::string> tokenize(std::string list)
    {
        std::string token;
        std::istringstream stream(list);
        std::vector<std::string> result;

        while(stream >> token)
        {
            if (token.size())
                result.push_back(token);
        }

        return result;
    }

    void reload_widgets(std::string list, WidgetContainer& container,
                        Gtk::HBox& box)
    {
        container.clear();
        auto widgets = tokenize(list);
        for (auto widget_name : widgets)
        {
            auto widget = widget_from_name(widget_name);
            if (!widget)
                continue;

            widget->widget_name = widget_name;
            widget->init(&box, app->config.get());
            container.push_back(std::move(widget));
        }

        box.show_all();
    }

    wf_option left_widgets_opt, right_widgets_opt, center_widgets_opt;
    wf_option_callback left_widgets_updated, right_widgets_updated,
                       center_widgets_updated;

    void init_widgets()
    {
        auto section = app->config->get_section("panel");
        left_widgets_opt = section->get_option("widgets_left", "launchers");
        right_widgets_opt = section->get_option("widgets_right", "network battery");
        center_widgets_opt = section->get_option("widgets_center", "clock");

        left_widgets_updated = [=] () {
            reload_widgets(left_widgets_opt->as_string(), left_widgets, left_box);
        };
        right_widgets_updated = [=] () {
            reload_widgets(right_widgets_opt->as_string(), right_widgets, right_box);
        };
        center_widgets_updated = [=] () {
            reload_widgets(center_widgets_opt->as_string(), center_widgets, center_box);
        };

        left_widgets_opt->add_updated_handler(&left_widgets_updated);
        right_widgets_opt->add_updated_handler(&right_widgets_updated);
        center_widgets_opt->add_updated_handler(&center_widgets_updated);

        left_widgets_updated();
        right_widgets_updated();
        center_widgets_updated();
    }

    public:
    WayfirePanel(WayfireShellApp *app, WayfireOutput *output)
    {
        this->app = app;
        this->output = output;
        autohide_opt = app->config->get_section("panel")->get_option("autohide", "1");
        autohide_opt->add_updated_handler(&update_autohide);

        panel_position = app->config->get_section("panel")->get_option("position", "top");

        setup_window();
        init_layout();
        init_widgets();

        output->resized_callback = [=] (WayfireOutput*, uint32_t w, uint32_t h)
        {
            handle_output_resize(w, h);
        };
    }

    ~WayfirePanel()
    {
        autohide_opt->rem_updated_handler(&update_autohide);
        bg_color->rem_updated_handler(&background_callback);

        left_widgets_opt->rem_updated_handler(&left_widgets_updated);
        right_widgets_opt->rem_updated_handler(&right_widgets_updated);
        center_widgets_opt->rem_updated_handler(&center_widgets_updated);
    }

    void handle_config_reload()
    {
        for (auto& w : left_widgets)
            w->handle_config_reload(app->config.get());
        for (auto& w : right_widgets)
            w->handle_config_reload(app->config.get());
        for (auto& w : center_widgets)
            w->handle_config_reload(app->config.get());

        update_position();
    }
};

class WayfirePanelApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfirePanel> > panels;

    public:
    WayfirePanelApp(int argc, char **argv)
        : WayfireShellApp(argc, argv)
    {
    }

    void on_config_reload()
    {
        for (auto& p : panels)
            p.second->handle_config_reload();
    }

    void on_new_output(WayfireOutput *output)
    {
        panels[output] = std::unique_ptr<WayfirePanel> (
            new WayfirePanel(this, output));
    }

    void on_output_removed(WayfireOutput *output)
    {
        panels.erase(output);
    }
};

int main(int argc, char **argv)
{
    WayfirePanelApp panel_app(argc, argv);
    panel_app.run();
    return 0;
}
