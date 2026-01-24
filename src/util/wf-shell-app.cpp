#include "wf-shell-app.hpp"
#include <glibmm/main.h>
#include <sys/inotify.h>
#include <gdk/wayland/gdkwayland.h>
#include <gio/gio.h>
#include <iostream>
#include <filesystem>
#include <memory>
#include <wayfire/config/file.hpp>
#include <wf-option-wrap.hpp>
#include <gtk-utils.hpp>

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
    std::filesystem::create_directories(css_directory);

    return css_directory;
}

void WayfireShellApp::on_css_reload()
{
    clear_css_rules();
    /* Add our defaults */
    add_css_file((std::string)RESOURCEDIR + "/css/default.css", GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    /* Add user directory */
    std::string ext(".css");
    for (auto & p : std::filesystem::directory_iterator(get_css_config_dir()))
    {
        if (p.path().extension() == ext)
        {
            add_css_file(p.path().string(), GTK_STYLE_PROVIDER_PRIORITY_USER);
        }
    }

    /* Add one user file */
    auto custom_css_config = WfOption<std::string>{"panel/css_path"};
    std::string custom_css = custom_css_config;
    if (custom_css != "")
    {
        add_css_file(custom_css, GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
}

void WayfireShellApp::clear_css_rules()
{
    auto display = Gdk::Display::get_default();
    for (auto css_provider : css_rules)
    {
        Gtk::StyleContext::remove_provider_for_display(display, css_provider);
    }

    css_rules.clear();
}

void WayfireShellApp::add_css_file(std::string file, int priority)
{
    auto display = Gdk::Display::get_default();
    if (file != "")
    {
        auto css_provider = load_css_from_path(file);
        if (css_provider)
        {
            Gtk::StyleContext::add_provider_for_display(
                display, css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
            css_rules.push_back(css_provider);
        }
    }
}

bool WayfireShellApp::parse_cfgfile(const Glib::ustring & option_name,
    const Glib::ustring & value, bool has_value)
{
    std::cout << "%%%%%%%%%%%%%%%%%%%%%%%" << std::endl;
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
}

/* Reload file and add next inotify watch */
static void do_reload_config(WayfireShellApp *app)
{
    wf::config::load_configuration_options_from_file(
        app->config, app->get_config_file());
    app->on_config_reload();
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
    if(activated)
    {
        return;
    }
    activated = true;
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

    inotify_add_watch(inotify_fd,
        get_config_file().c_str(),
        IN_CLOSE_WRITE);
    inotify_add_watch(inotify_css_fd,
        get_css_config_dir().c_str(),
        IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE);
    Glib::signal_io().connect(
        sigc::bind<0>(&handle_inotify_event, this),
        inotify_fd, Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
    Glib::signal_io().connect(
        sigc::bind<0>(&handle_css_inotify_event, this),
        inotify_css_fd, Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);

    if (!alternative_monitors)
    {
        // Hook up monitor tracking
        auto display  = Gdk::Display::get_default();
        auto monitors = display->get_monitors();
        monitors->signal_items_changed().connect(sigc::mem_fun(*this, &WayfireShellApp::output_list_updated));

        // initial monitors
        int num_monitors = monitors->get_n_items();
        for (int i = 0; i < num_monitors; i++)
        {
            auto obj = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(i));
            add_output(obj);
        }
    }
}

void WayfireShellApp::output_list_updated(const int pos, const int rem, const int add)
{
    auto display  = Gdk::Display::get_default();
    auto monitors = display->get_monitors();
    for (int i = 0; i < add; i++)
    {
        auto obj = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(i + pos));
        add_output(obj);
    }
}

void WayfireShellApp::add_output(GMonitor monitor)
{
    auto it = std::find_if(monitors.begin(), monitors.end(),
        [monitor] (auto& output) { return output->monitor == monitor; });

    if (it != monitors.end())
    {
        // We have an entry for this output
        return;
    }

    // Remove self when unplugged
    monitor->signal_invalidate().connect([=]
    {
        rem_output(monitor);
    });
    // Add to list
    monitors.push_back(
        std::make_unique<WayfireOutput>(monitor, this->wf_shell_manager));
    handle_new_output(monitors.back().get());
}

void WayfireShellApp::rem_output(GMonitor monitor)
{
    auto it = std::find_if(monitors.begin(), monitors.end(),
        [monitor] (auto& output) { return output->monitor == monitor; });

    if (it != monitors.end())
    {
        handle_output_removed(it->get());
        monitors.erase(it);
    }

}

Gio::Application::Flags WayfireShellApp::get_extra_application_flags()
{
    return Gio::Application::Flags::NONE;
}

WayfireShellApp::WayfireShellApp()
{
}

void WayfireShellApp::init_app()
{
    std::cout << "setting up" << std::endl;
    app = Gtk::Application::create(this->get_application_name(), Gio::Application::Flags::HANDLES_COMMAND_LINE | this->get_extra_application_flags());
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

void WayfireShellApp::run(int argc, char **argv)
{
    app->run(argc, argv);
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
