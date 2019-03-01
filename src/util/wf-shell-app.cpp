#include "wf-shell-app.hpp"
#include <glibmm/main.h>
#include <sys/inotify.h>

#include <unistd.h>

std::string WayfireShellApp::get_config_file()
{
    std::string home_dir = getenv("HOME");
    std::string config_file = home_dir + "/.config/wf-shell.ini";
    return config_file;
}

#define INOT_BUF_SIZE (1024 * sizeof(inotify_event))
char buf[INOT_BUF_SIZE];

static void do_reload_config(WayfireShellApp *app)
{
    app->config->reload_config();
    app->on_config_reload();
    inotify_add_watch(app->inotify_fd, app->get_config_file().c_str(), IN_MODIFY);
}

static bool handle_inotify_event(WayfireShellApp *app, Glib::IOCondition cond)
{
    /* read, but don't use */
    read(app->inotify_fd, buf, INOT_BUF_SIZE);
    do_reload_config(app);

    return true;
}

void WayfireShellApp::on_activate()
{
    app->hold();

    // setup config
    config = std::unique_ptr<wayfire_config> (
        new wayfire_config(get_config_file()));

    inotify_fd = inotify_init();
    do_reload_config(this);

    Glib::signal_io().connect(
        sigc::bind<0>(&handle_inotify_event, this),
        inotify_fd, Glib::IO_IN | Glib::IO_HUP);

    // setup display
    auto handle_new_output = [=] (WayfireOutput *output)
    {
        output->destroyed_callback = [=] (WayfireOutput *output) {
            on_output_removed(output);
        };

        on_new_output(output);
    };
    display = std::unique_ptr<WayfireDisplay> (new WayfireDisplay(handle_new_output));
}

WayfireShellApp::WayfireShellApp(int argc, char **argv)
{
    app = Gtk::Application::create(argc, argv);
    app->signal_activate().connect_notify(
        sigc::mem_fun(this, &WayfireShellApp::on_activate));
}

void WayfireShellApp::run()
{
    app->run();
}
