#include <glibmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/application.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>
#include <gtk-layer-shell.h>

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
#include "widgets/volume.hpp"
#include "widgets/window-list/window-list.hpp"

#include "wf-shell-app.hpp"
#include "wf-autohide-window.hpp"

struct WayfirePanelZwfOutputCallbacks
{
    std::function<void()> enter_fullscreen;
    std::function<void()> leave_fullscreen;
};

static void handle_zwf_output_enter_fullscreen(void* dd,
    zwf_output_v2 *zwf_output_v2)
{
    auto data = (WayfirePanelZwfOutputCallbacks*)
        zwf_output_v2_get_user_data(zwf_output_v2);

    if (data)
        data->enter_fullscreen();
}

static void handle_zwf_output_leave_fullscreen(void *,
    zwf_output_v2 *zwf_output_v2)
{
    auto data = (WayfirePanelZwfOutputCallbacks*)
        zwf_output_v2_get_user_data(zwf_output_v2);

    if (data)
        data->leave_fullscreen();
}

static struct zwf_output_v2_listener output_impl = {
    .enter_fullscreen = handle_zwf_output_enter_fullscreen,
    .leave_fullscreen = handle_zwf_output_leave_fullscreen,
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

    int last_autohide_value = -1;
    wf_option autohide_opt;
    wf_option_callback autohide_opt_updated = [=] ()
    {
        int is_autohide = !!autohide_opt->as_int();
        if (is_autohide == last_autohide_value)
            return;

        if (is_autohide) {
            this->window->increase_autohide();
        } else if (last_autohide_value == 1) {
            this->window->decrease_autohide();
        }

        last_autohide_value = is_autohide;
        window->set_auto_exclusive_zone(!is_autohide);
    };

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

    wf_option panel_layer;
    wf_option_callback set_panel_layer = [=] ()
    {
        if (panel_layer->as_string() == "overlay")
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
        if (panel_layer->as_string() == "top")
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);
        if (panel_layer->as_string() == "bottom")
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_BOTTOM);
        if (panel_layer->as_string() == "background")
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    };

    wf_option minimal_panel_height;
    void create_window()
    {
        auto config_section =
            WayfirePanelApp::get().get_config()->get_section("panel");

        minimal_panel_height = config_section->get_option("minimal_height",
            DEFAULT_PANEL_HEIGHT);

        window = std::make_unique<WayfireAutohidingWindow> (output);
        window->set_size_request(1, minimal_panel_height->as_int());
        panel_layer = config_section->get_option("layer", "overlay");
        panel_layer->add_updated_handler(&set_panel_layer);
        set_panel_layer(); // initial setting

        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);

        bg_color = config_section->get_option("background_color", "gtk_headerbar");
        bg_color->add_updated_handler(&on_window_color_updated);
        on_window_color_updated(); // set initial color

        autohide_opt = config_section->get_option("autohide", "1");
        autohide_opt->add_updated_handler(&autohide_opt_updated);
        autohide_opt_updated(); // set initial autohide status

        window->set_position(
            config_section->get_option("position", PANEL_POSITION_TOP));

        window->show_all();
        init_widgets();
        init_layout();

        window->signal_delete_event().connect(
            sigc::mem_fun(this, &WayfirePanel::impl::on_delete));
    }

    bool on_delete(GdkEventAny *ev)
    {
        /* We ignore close events, because the panel's lifetime is bound to
         * the lifetime of the output */
        return true;
    }

    void init_layout()
    {
        content_box.pack_start(left_box, false, false);
        std::vector<Gtk::Widget*> center_children = center_box.get_children();
        if (center_children.size() > 0)
            content_box.set_center_widget(center_box);
        content_box.pack_end(right_box, false, false);
        window->add(content_box);
        window->show_all();
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
        if (name == "volume")
            return Widget(new WayfireVolume());
        if (name == "window-list")
            return Widget(new WayfireWindowList(output));

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

    WayfirePanelZwfOutputCallbacks callbacks;

    public:
    impl(WayfireOutput *output)
    {
        this->output = output;
        create_window();

        if (output->output)
        {
            callbacks.enter_fullscreen = [=]() { window->increase_autohide(); };
            callbacks.leave_fullscreen = [=]() { window->decrease_autohide(); };
            zwf_output_v2_add_listener(output->output, &output_impl, NULL);
            zwf_output_v2_set_user_data(output->output, &callbacks);
        }
    }

    ~impl()
    {
        if (output->output)
            zwf_output_v2_set_user_data(output->output, NULL);

        panel_layer->rem_updated_handler(&set_panel_layer);
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

    Gtk::Window& get_window()
    {
        return *window;
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
Gtk::Window& WayfirePanel::get_window() { return pimpl->get_window(); }
void WayfirePanel::handle_config_reload() { return pimpl->handle_config_reload(); }

class WayfirePanelApp::impl : public WayfireShellApp
{
    public:
    std::map<WayfireOutput*, std::unique_ptr<WayfirePanel> > panels;

    impl(int argc, char **argv) : WayfireShellApp(argc, argv) { }
    void on_config_reload() override
    {
        for (auto& p : panels)
            p.second->handle_config_reload();
    }

    void handle_new_output(WayfireOutput *output) override
    {
        panels[output] = std::unique_ptr<WayfirePanel> (
            new WayfirePanel(output));
    }

    WayfirePanel* panel_for_wl_output(wl_output *output)
    {
        for (auto& p : panels)
        {
            if (p.first->wo == output)
                return p.second.get();
        }

        return nullptr;
    }

    void handle_output_removed(WayfireOutput *output) override
    {
        panels.erase(output);
    }
};

WayfirePanel* WayfirePanelApp::panel_for_wl_output(wl_output *output) { return pimpl->panel_for_wl_output(output); }
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
