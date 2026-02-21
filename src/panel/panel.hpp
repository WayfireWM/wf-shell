#pragma once

#include <memory>
#include <wayland-client.h>
#include <gtkmm/window.h>
#include <gtkmm/cssprovider.h>

#include "giomm/application.h"
#include "css-config.hpp"
#include "giomm/application.h"
#include "wf-shell-app.hpp"
#include "wf-ipc.hpp"

class WayfirePanelApp;
class WayfirePanel
{
  public:
    WayfirePanel(WayfireOutput *output);

    wl_surface *get_wl_surface();
    Gtk::Window& get_window();
    void handle_config_reload();
    void init_widgets();
    void set_panel_app(WayfirePanelApp *panel_app);
    std::shared_ptr<WayfireIPC> get_ipc_server_instance();

  private:
    class impl;
    std::unique_ptr<impl> pimpl;
};

class WayfirePanelApp : public WayfireShellApp
{
  public:
    WayfirePanel *panel_for_wl_output(wl_output *output);
    static WayfirePanelApp& get();
    std::string get_application_name() override;
    Gio::Application::Flags get_extra_application_flags() override;

    /* Starts the program. get() is valid afterward the first (and the only)
     * call to create() */
    static void create(int argc, char **argv);
    ~WayfirePanelApp();

    void on_activate() override;
    void handle_new_output(WayfireOutput *output) override;
    void handle_output_removed(WayfireOutput *output) override;
    bool panel_allowed_by_config(bool allowed, std::string output_name);
    void on_config_reload() override;
    void reload_css();
    std::shared_ptr<WayfireIPC> get_ipc_server_instance();
    std::shared_ptr<WayfireIPC> ipc_server;

  private:
    WayfirePanelApp();

    class impl;
    std::unique_ptr<impl> priv;
};
