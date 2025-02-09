#include "wf-shell-app.hpp"
#include <glibmm/main.h>
#include <sys/inotify.h>
#include <gdk/wayland/gdkwayland.h>
#include <gio/gio.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include <wayfire/config/file.hpp>

#include <unistd.h>

std::string WayfireShellApp::get_config_file()
{
    if (cmdline_config.has_value())
    {
        return cmdline_config.value();
    }

    std::string config_dir;

    char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home == NULL)
    {
        config_dir = std::string(getenv("HOME")) + "/.config";
    } else
    {
        config_dir = std::string(config_home);
    }

    return config_dir + "/wf-shell.ini";
}

std::string WayfireShellApp::get_css_config_dir()
{
    if (cmdline_css.has_value())
    {
        return cmdline_config.value();
    }

    std::string config_dir;

    char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home == NULL)
    {
        config_dir = std::string(getenv("HOME")) + "/.config";
    } else
    {
        config_dir = std::string(config_home);
    }

    auto css_directory = config_dir + "/wf-shell/css/";
    /* Ensure it exists */
    bool created = std::filesystem::create_directories(css_directory);
    if (created)
    {
        std::string default_css = (std::string)RESOURCEDIR + "/css/default.css";
        std::string destination = css_directory + "default.css";
        if (std::filesystem::exists(default_css))
        {
            std::filesystem::copy(default_css, destination);
        }
    }

    return css_directory;
}

bool WayfireShellApp::parse_cfgfile(const Glib::ustring & option_name,
    const Glib::ustring & value, bool has_value)
{
    std::cout << "Using custom config file " << value << std::endl;
    cmdline_config = value;
    return true;
}

bool WayfireShellApp::parse_cssfile(const Glib::ustring & option_name,
    const Glib::ustring & value, bool has_value)
{
    std::cout << "Using custom css directory " << value << std::endl;
    cmdline_css = value;
    return true;
}

#define INOT_BUF_SIZE (1024 * sizeof(inotify_event))
char buf[INOT_BUF_SIZE];

static void do_reload_css(WayfireShellApp *app)
{
    app->on_css_reload();
    inotify_add_watch(app->inotify_css_fd,
        app->get_css_config_dir().c_str(),
        IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE);
}

/* Reload file and add next inotify watch */
static void do_reload_config(WayfireShellApp *app)
{
    wf::config::load_configuration_options_from_file(
        app->config, app->get_config_file());
    app->on_config_reload();
    inotify_add_watch(app->inotify_fd, app->get_config_file().c_str(), IN_MODIFY);
}

/* Handle inotify event */
static bool handle_inotify_event(WayfireShellApp *app, Glib::IOCondition cond)
{
    /* read, but don't use */
    read(app->inotify_fd, buf, INOT_BUF_SIZE);
    do_reload_config(app);

    return true;
}

static bool handle_css_inotify_event(WayfireShellApp *app, Glib::IOCondition cond)
{
    /* read, but don't use */
    read(app->inotify_css_fd, buf, INOT_BUF_SIZE);
    do_reload_css(app);

    return true;
}

static void registry_add_object(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
    auto app = static_cast<WayfireShellApp*>(data);
    if (strcmp(interface, zwf_shell_manager_v2_interface.name) == 0)
    {
        app->wf_shell_manager = (zwf_shell_manager_v2*)wl_registry_bind(registry, name,
            &zwf_shell_manager_v2_interface, std::min(version, 2u));
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry,
    uint32_t name)
{}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

void WayfireShellApp::on_activate()
{
    app->hold();

    // load wf-shell if available
    auto gdk_display = gdk_display_get_default();
    auto wl_display  = gdk_wayland_display_get_wl_display(gdk_display);
    if (!wl_display)
    {
        std::cerr << "Failed to connect to wayland display!" <<
            " Are you sure you are running a wayland compositor?" << std::endl;
        std::exit(-1);
    }

    wl_registry *registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(wl_display);

    std::vector<std::string> xmldirs(1, METADATA_DIR);

    // setup config
    this->config = wf::config::build_configuration(
        xmldirs, SYSCONF_DIR "/wayfire/wf-shell-defaults.ini",
        get_config_file());

    inotify_fd = inotify_init();
    do_reload_config(this);
    inotify_css_fd = inotify_init();
    do_reload_css(this);

    Glib::signal_io().connect(
        sigc::bind<0>(&handle_inotify_event, this),
        inotify_fd, Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
    Glib::signal_io().connect(
        sigc::bind<0>(&handle_css_inotify_event, this),
        inotify_css_fd, Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);

    // Hook up monitor tracking
    auto display = Gdk::Display::get_default();
    auto monitors = display->get_monitors();
    monitors->signal_items_changed().connect(
        [=] (const int position, const int removed, const int added) {
            //this->add_output(monitor);
            // TODO Reconstitute list correctly
        });
    //display->signal_monitor_removed().connect_notify(
    //    [=] (const GMonitor& monitor) { this->rem_output(monitor); });

    // initial monitors
    int num_monitors = monitors->get_n_items();
    for (int i = 0; i < num_monitors; i++)
    {
        auto obj = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(i));
        add_output(obj);
    }
}

void WayfireShellApp::add_output(GMonitor monitor)
{
    monitors.push_back(
        std::make_unique<WayfireOutput>(monitor, this->wf_shell_manager));
    handle_new_output(monitors.back().get());
}

void WayfireShellApp::rem_output(GMonitor monitor)
{
    auto it = std::remove_if(monitors.begin(), monitors.end(),
        [monitor] (auto& output) { return output->monitor == monitor; });

    if (it != monitors.end())
    {
        handle_output_removed(it->get());
        monitors.erase(it, monitors.end());
    }
}

WayfireShellApp::WayfireShellApp(int argc, char **argv)
{
    app = Gtk::Application::create("",Gio::Application::Flags::HANDLES_COMMAND_LINE);
    app->signal_activate().connect(
        sigc::mem_fun(*this, &WayfireShellApp::on_activate));
    app->add_main_option_entry(
        sigc::mem_fun(*this, &WayfireShellApp::parse_cfgfile),
        "config", 'c', "config file to use", "file");
    app->add_main_option_entry(
        sigc::mem_fun(*this, &WayfireShellApp::parse_cssfile),
        "css", 's', "css style directory to use", "directory");

    // Activate app after parsing command line
    app->signal_command_line().connect_notify([=] (auto&)
    {
        app->activate();
    });
}

WayfireShellApp::~WayfireShellApp()
{}

std::unique_ptr<WayfireShellApp> WayfireShellApp::instance;
WayfireShellApp& WayfireShellApp::get()
{
    return *instance;
}

void WayfireShellApp::run()
{
    app->run();
}

/* -------------------------- WayfireOutput --------------------------------- */
WayfireOutput::WayfireOutput(const GMonitor& monitor,
    zwf_shell_manager_v2 *zwf_manager)
{
    this->monitor = monitor;
    this->wo = gdk_wayland_monitor_get_wl_output(monitor->gobj());

    if (zwf_manager)
    {
        this->output =
            zwf_shell_manager_v2_get_wf_output(zwf_manager, this->wo);
    } else
    {
        this->output = nullptr;
    }
}

WayfireOutput::~WayfireOutput()
{
    if (this->output)
    {
        zwf_output_v2_destroy(this->output);
    }
}

sigc::signal<void()> WayfireOutput::toggle_menu_signal()
{
    return m_toggle_menu_signal;
}
