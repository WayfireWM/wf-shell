#include <glibmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/application.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>

#include <stdio.h>
#include <iostream>
#include <sys/inotify.h>

#include "widgets/battery.hpp"
#include "widgets/clock.hpp"
#include "widgets/launchers.hpp"
#include "display.hpp"

class WayfirePanel;
namespace
{
    int inotify_fd;

    WayfireDisplay *display = NULL;
    wayfire_config *panel_config;

    std::vector<std::unique_ptr<WayfirePanel>> panels;
}

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
    Gtk::Window window;

    Gtk::HBox content_box;
    Gtk::HBox left_box, center_box, right_box;

    WayfireWidget *widget, *widget1, *widget2;
    WayfireOutput *output;
    zwf_wm_surface_v1 *wm_surface = NULL;


    int hidden_height = 1;
    int get_hidden_y()
    {
        return hidden_height - window.get_allocated_height();
    }

    wf_option duration = new_static_option("300");
    wf_duration transition{duration};

    bool animation_running = false;
    bool update_position()
    {
        if (animation_running || transition.running())
        {
            zwf_wm_surface_v1_configure(wm_surface, 0, std::round(transition.progress()));
            window.queue_draw();

            return (animation_running = transition.running());
        }

        return false;
    }

    bool show()
    {
        int start = transition.progress();
        transition.start(start, 0);
        update_position();
        return false; // disconnect
    }

    bool hide()
    {
        int start = transition.progress();
        transition.start(start, get_hidden_y());
        update_position();
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

    int reserved_area = -1;
    void update_reserved_area()
    {
        if (!wm_surface)
            return;

        if (autohide_enabled() && reserved_area != 0)
        {
            zwf_wm_surface_v1_set_exclusive_zone(
                wm_surface, ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP, 0);
            reserved_area = 0;
        }
        else if (!autohide_enabled() && reserved_area != window.get_height())
        {
            zwf_wm_surface_v1_set_exclusive_zone(
                wm_surface, ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP,
                window.get_height());
            reserved_area = window.get_height();
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
                                                  ZWF_OUTPUT_V1_WM_ROLE_PANEL);
        zwf_wm_surface_v1_set_exclusive_zone(
            wm_surface, ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP,
            window.get_height());

        zwf_output_v1_add_listener(output->zwf, &zwf_output_impl, &update_autohide_request);
    }

    void handle_output_resize(uint32_t width, uint32_t height)
    {
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
        update_position();
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

    void on_resized(Gtk::Allocation& alloc)
    {
        update_reserved_area();
    }

    void setup_window()
    {
        window.set_resizable(false);
        window.set_decorated(false);
        auto bg_color = panel_config->get_section("panel")
            ->get_option("background_color", "gtk_default");

        if (bg_color->as_string() != "gtk_default")
        {
            Gdk::RGBA rgba;
            auto color_string = bg_color->as_string();

            /* see if it is in #XXXXXX format */
            if (color_string.size() && color_string[0] == '#')
            {
                rgba.set(color_string);
            } else {
                /* otherwise, simply a list of double values, parse by default */
                auto color = bg_color->as_color();
                rgba.set_red(color.r);
                rgba.set_green(color.g);
                rgba.set_blue(color.b);
                rgba.set_alpha(color.a);
            }

            window.override_background_color(rgba);
        }

        window.signal_draw().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_draw));
        window.signal_enter_notify_event().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_enter));
        window.signal_leave_notify_event().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_leave));
        window.signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WayfirePanel::on_resized));
    }

    void init_layout()
    {
        content_box.pack_start(left_box, false, false);
        content_box.set_center_widget(center_box);
        content_box.pack_end(right_box, false, false);
        window.add(content_box);
    }

    void init_widgets()
    {
        widget = new WayfireClock();
        widget->init(&center_box, panel_config);

        widget1 = new WayfireLaunchers();
        widget1->init(&left_box, panel_config);

        widget2 = new WayfireBatteryInfo();
        widget2->init(&right_box, panel_config);
    }

    public:
    WayfirePanel(WayfireOutput *output)
    {
        this->output = output;
        autohide_opt = panel_config->get_section("panel")->get_option("autohide", "1");
        autohide_opt->add_updated_handler(&update_autohide);

        setup_window();
        init_layout();
        init_widgets();

        output->resized_callback = [=] (WayfireOutput*, uint32_t w, uint32_t h)
        {
            handle_output_resize(w, h);
        };

        output->destroyed_callback = [=] (WayfireOutput *output)
        {
            delete this;
        };
    }

    ~WayfirePanel()
    {
        autohide_opt->rem_updated_handler(&update_autohide);
    }

    void handle_config_reload()
    {
        widget->handle_config_reload(panel_config);
        widget1->handle_config_reload(panel_config);
        widget2->handle_config_reload(panel_config);
    }
};

std::string get_config_file()
{
    std::string home_dir = secure_getenv("HOME");
    std::string config_file = home_dir + "/.config/wf-shell.ini";
    return config_file;
}

#define INOT_BUF_SIZE (1024 * sizeof(inotify_event))
char buf[INOT_BUF_SIZE];

static void reload_config(int fd)
{
    panel_config->reload_config();

    for (auto& panel : panels)
        panel->handle_config_reload();

    inotify_add_watch(fd, get_config_file().c_str(), IN_MODIFY);
}

static bool on_config_reload(Glib::IOCondition)
{
    /* read, but don't use */
    read(inotify_fd, buf, INOT_BUF_SIZE);
    reload_config(inotify_fd);

    return true;
}

void on_activate(Glib::RefPtr<Gtk::Application> app)
{
    app->hold();

    auto handle_new_output = [] (WayfireOutput *output)
    {
        panels.push_back(std::unique_ptr<WayfirePanel> (new WayfirePanel(output)));
    };

    panel_config = new wayfire_config(get_config_file());

    inotify_fd = inotify_init();
    reload_config(inotify_fd);

    Glib::signal_io().connect(sigc::ptr_fun(&on_config_reload),
                              inotify_fd, Glib::IO_IN | Glib::IO_HUP);

    display = new WayfireDisplay(handle_new_output);
}

int main(int argc, char **argv)
{
    auto app = Gtk::Application::create(argc, argv);
    app->signal_activate().connect(sigc::bind(&on_activate, app));
    app->run();

    if (display)
        delete display;

    return 0;
}
