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

#include "panel.hpp"

#include "widgets/battery.hpp"
#include "widgets/menu.hpp"
#include "widgets/clock.hpp"
#include "widgets/launchers.hpp"
#include "widgets/network.hpp"
#include "widgets/spacing.hpp"

#include "wf-shell-app.hpp"
#include "wf-autohide-window.hpp"

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

class WayfirePanel::impl
{
    std::unique_ptr<WayfireAutohidingWindow> window;

    Gtk::HBox content_box;
    Gtk::HBox left_box, center_box, right_box;

    using Widget = std::unique_ptr<WayfireWidget>;
    using WidgetContainer = std::vector<Widget>;
    WidgetContainer left_widgets, center_widgets, right_widgets;

    WayfireOutput *output;
    int current_output_width = 0, current_output_height = 0;

    bool was_autohide_enabled = false;
    wf_option autohide_opt;
    wf_option_callback autohide_opt_updated = [=] ()
    {
        bool is_autohide = autohide_opt->as_int();
        if (is_autohide == was_autohide_enabled)
            return;

        was_autohide_enabled = is_autohide;
        update_autohide_request(is_autohide);
        window->set_exclusive_zone(!is_autohide);
    };

    std::function<void(bool)> update_autohide_request = [=] (bool autohide)
    {
        /* FIXME: will break if panel is created while there are fullscreen windows */
        if (!this->window)
            return;

        if (autohide) {
            this->window->increase_autohide();
        } else {
            this->window->decrease_autohide();
        }
    };

    wf_option minimal_panel_height;
    void handle_output_resize(uint32_t width, uint32_t height)
    {
        this->current_output_width = width;
        this->current_output_height = height;

        if (!this->window)
            create_window();

        this->window->set_size_request(current_output_width,
            minimal_panel_height->as_int());
    }

    wf_option bg_color;
    wf_option_callback on_window_color_updated = [=] ()
    {
        if (bg_color->as_string() == "gtk_default")
            return window->unset_background_color();

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

        window->override_background_color(rgba);
    };

    void create_window()
    {
        auto config_section =
            WayfirePanelApp::get().get_config()->get_section("panel");
        minimal_panel_height = config_section->get_option("minimal_height",
            DEFAULT_PANEL_HEIGHT);

        window = std::unique_ptr<WayfireAutohidingWindow> (
            new WayfireAutohidingWindow(
                this->current_output_width,
                minimal_panel_height->as_int(),
                this->output,
                ZWF_WM_SURFACE_V1_ROLE_OVERLAY));

        bg_color = config_section->get_option("background_color", "gtk_headerbar");
        bg_color->add_updated_handler(&on_window_color_updated);
        on_window_color_updated(); // set initial color

        autohide_opt = config_section->get_option("autohide", "1");
        autohide_opt->add_updated_handler(&autohide_opt_updated);
        /* Make sure that we trigger an autohide opt update. We haven't set any
         * autohide state up to now, so this won't corrupt the autohide counters */
        was_autohide_enabled = !autohide_opt->as_int();
        autohide_opt_updated(); // set initial autohide status

        window->set_position(
            config_section->get_option("position", PANEL_POSITION_TOP));

        init_layout();
        init_widgets();

        window->signal_delete_event().connect(
            sigc::mem_fun(this, &WayfirePanel::impl::on_delete));
        window->signal_focus_out_event().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::impl::on_focus_out));
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
        if (window->is_autohide())
            window->schedule_hide(100);
    }

    void init_layout()
    {
        content_box.pack_start(left_box, false, false);
        content_box.set_center_widget(center_box);
        content_box.pack_end(right_box, false, false);
        window->add(content_box);
        content_box.show_all();
    }

    Widget widget_from_name(std::string name)
    {
        if (name == "menu")
            return Widget(new WayfireMenu());
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

        if (name != "none")
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
            widget->init(&box, WayfirePanelApp::get().get_config());
            container.push_back(std::move(widget));
        }
    }

    wf_option left_widgets_opt, right_widgets_opt, center_widgets_opt;
    wf_option_callback left_widgets_updated, right_widgets_updated,
                       center_widgets_updated;

    void init_widgets()
    {
        auto section = WayfirePanelApp::get().get_config()->get_section("panel");
        left_widgets_opt = section->get_option("widgets_left", "menu spacing18 launchers");
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
    impl(WayfireOutput *output)
    {
        this->output = output;
        zwf_output_v1_add_listener(output->zwf, &zwf_output_impl, &update_autohide_request);

        output->resized_callback = [=] (WayfireOutput*, uint32_t w, uint32_t h)
        {
            handle_output_resize(w, h);
        };
    }

    ~impl()
    {
        autohide_opt->rem_updated_handler(&autohide_opt_updated);
        bg_color->rem_updated_handler(&on_window_color_updated);

        left_widgets_opt->rem_updated_handler(&left_widgets_updated);
        right_widgets_opt->rem_updated_handler(&right_widgets_updated);
        center_widgets_opt->rem_updated_handler(&center_widgets_updated);
    }

    wl_surface *get_wl_surface()
    {
        return window->get_wl_surface();
    }

    void handle_config_reload()
    {
        auto config = WayfirePanelApp::get().get_config();
        for (auto& w : left_widgets)
            w->handle_config_reload(config);
        for (auto& w : right_widgets)
            w->handle_config_reload(config);
        for (auto& w : center_widgets)
            w->handle_config_reload(config);
    }
};

WayfirePanel::WayfirePanel(WayfireOutput *output) : pimpl(new impl(output)) { }
wl_surface *WayfirePanel::get_wl_surface() { return pimpl->get_wl_surface(); }
void WayfirePanel::handle_config_reload() { return pimpl->handle_config_reload(); }

class WayfirePanelApp::impl : public WayfireShellApp
{
    public:
    std::map<WayfireOutput*, std::unique_ptr<WayfirePanel> > panels;

    impl(int argc, char **argv) : WayfireShellApp(argc, argv) { }
    void on_config_reload()
    {
        for (auto& p : panels)
            p.second->handle_config_reload();
    }

    void on_new_output(WayfireOutput *output)
    {
        panels[output] = std::unique_ptr<WayfirePanel> (
            new WayfirePanel(output));
    }

    WayfirePanel* panel_for_wl_output(wl_output *output)
    {
        for (auto& p : panels)
        {
            if (p.first->handle == output)
                return p.second.get();
        }

        return nullptr;
    }

    void on_output_removed(WayfireOutput *output)
    {
        panels.erase(output);
    }
};

WayfirePanel* WayfirePanelApp::panel_for_wl_output(wl_output *output) { return pimpl->panel_for_wl_output(output); }
WayfireDisplay *WayfirePanelApp::get_display() { return pimpl->display.get(); }
wayfire_config *WayfirePanelApp::get_config() { return pimpl->config.get(); }

std::unique_ptr<WayfirePanelApp> WayfirePanelApp::instance;
WayfirePanelApp& WayfirePanelApp::get()
{
    if (!instance)
        throw std::logic_error("Calling WayfirePanelApp::get() before starting app!");
    return *instance.get();
}

void WayfirePanelApp::run(int argc, char **argv)
{
    if (instance)
        throw std::logic_error("Running WayfirePanelApp twice!");

    instance = std::unique_ptr<WayfirePanelApp>{new WayfirePanelApp(argc, argv)};
    instance->pimpl->run();
}

WayfirePanelApp::~WayfirePanelApp() = default;
WayfirePanelApp::WayfirePanelApp(int argc, char **argv)
    : pimpl(new impl(argc, argv)) { }

int main(int argc, char **argv)
{
    WayfirePanelApp::run(argc, argv);
    return 0;
}
